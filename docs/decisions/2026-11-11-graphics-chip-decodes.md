# The graphics chip decodes the picture

- **Decided:** the host's picture is decoded by the computer's graphics chip only: VideoToolbox on a
  Mac, Direct3D 11 on Windows. H.265 where the chip decodes it, as a console takes it, and H.264
  everywhere else. There is no decoder in software.
- **Date:** 2026-11-11
- **Why:** nearly every Mac and PC has the chip for H.264, and most from about 2016 on for H.265. A
  decoder in software would bring ffmpeg into the app, and be slow where it would be needed.
  `2pstart --check-game` says when a computer lacks one.
- **Numbers:** #8, "Piece D: the game on the Mac"; #10, "Piece E: the game on Windows, and the zip".
- **Replaces:** none.
