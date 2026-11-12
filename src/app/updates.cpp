// Check for updates (updates.h).

#include "app/updates.h"

#include <cstdlib>
#include <vector>

#include <curl/curl.h>

#include "jsonutil.h"

namespace updates {

namespace {

size_t keep(char* data, size_t size, size_t count, void* to) {
    auto* s = static_cast<std::string*>(to);
    // A release is a few kilobytes; anything far bigger is not one.
    if (s->size() > 1024 * 1024) return 0;
    s->append(data, size * count);
    return size * count;
}

// The numbers between the dots: "0.10.2" is 0, 10, 2. Anything after a number's digits is left out.
std::vector<long> numbers(const std::string& v) {
    std::vector<long> out;
    size_t at = 0;
    while (at <= v.size()) {
        const size_t dot = v.find('.', at);
        const std::string part = v.substr(at, dot == std::string::npos ? std::string::npos : dot - at);
        out.push_back(std::strtol(part.c_str(), nullptr, 10));
        if (dot == std::string::npos) break;
        at = dot + 1;
    }
    return out;
}

}  // namespace

std::optional<Release> read(const std::string& answer) {
    json_object* o = jsonutil::parse(answer);
    if (!o || !json_object_is_type(o, json_type_object)) {
        if (o) json_object_put(o);
        return std::nullopt;
    }
    Release r;
    r.version = jsonutil::str(o, "tag_name");
    r.page = jsonutil::str(o, "html_url");
    json_object_put(o);
    if (!r.version.empty() && (r.version[0] == 'v' || r.version[0] == 'V')) r.version.erase(0, 1);
    // Only a page on GitHub is opened: the browser is given nothing else from the answer.
    if (r.version.empty() || r.page.rfind("https://github.com/", 0) != 0) return std::nullopt;
    return r;
}

bool newer(const std::string& theirs, const std::string& ours) {
    std::vector<long> a = numbers(theirs), b = numbers(ours);
    const size_t n = a.size() > b.size() ? a.size() : b.size();
    a.resize(n, 0);
    b.resize(n, 0);
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) return a[i] > b[i];
    }
    return false;
}

Check::Check(std::string ours, std::function<void()> wake) : ours_(std::move(ours)), wake_(std::move(wake)) {}

Check::~Check() {
    if (thread_.joinable()) thread_.join();
}

void Check::start() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (state_ == State::Checking) return;
        state_ = State::Checking;
    }
    if (thread_.joinable()) thread_.join();
    thread_ = std::thread([this] { run(); });
}

void Check::run() {
    std::string answer;
    long status = 0;
    bool reached = false;
    if (CURL* curl = curl_easy_init()) {
        curl_slist* headers = curl_slist_append(nullptr, "Accept: application/vnd.github+json");
        curl_easy_setopt(curl, CURLOPT_URL, kLatest);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, keep);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &answer);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
        // GitHub turns away a request with no name.
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "cabinetos-2p-start");
        reached = curl_easy_perform(curl) == CURLE_OK;
        if (reached) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    const std::optional<Release> release = reached && status == 200 ? read(answer) : std::nullopt;
    {
        std::lock_guard<std::mutex> lock(mu_);
        // No release yet is no update: the project has nothing published.
        if (reached && status == 404) state_ = State::UpToDate;
        else if (!release) state_ = State::Failed;
        else if (newer(release->version, ours_)) state_ = State::Available;
        else state_ = State::UpToDate;
        if (release) found_ = *release;
        ended_ = true;
    }
    if (wake_) wake_();
}

bool Check::ended() {
    std::lock_guard<std::mutex> lock(mu_);
    const bool was = ended_;
    ended_ = false;
    return was;
}

State Check::state() const {
    std::lock_guard<std::mutex> lock(mu_);
    return state_;
}

Release Check::found() const {
    std::lock_guard<std::mutex> lock(mu_);
    return found_;
}

}  // namespace updates
