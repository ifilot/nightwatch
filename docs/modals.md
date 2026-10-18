# Modal inventory

This lists the public file-manager and storage modal flows. Several flows share
one implementation: for example, filename patterns and directory paths use the
same input dialog, while errors and informational messages use one notice dialog.
External editors, launched programs and the DOS shell are separate processes.

| Modal | Entry point / purpose |
| --- | --- |
| Help | F1; scrollable embedded reference |
| About | Alt-F1, or A in Help; version and credits |
| Display mode | F2; display selection and remember-mode setting |
| Sort menu | Ctrl-S; name, size, date, extension and reversal |
| Drive picker | F9; configured drives and direct-path option |
| Directory/drive path | `.` or the path option inside F9 |
| Find filename pattern | Ctrl-F |
| Mark filename pattern | Ctrl-P |
| Unmark filename pattern | Ctrl-N |
| Copy destination | F5; absolute destination path |
| Move/rename destination | F6; absolute destination path |
| New directory name | F7 |
| Delete confirmation | F8; confirm file or recursive directory deletion |
| Directory-page confirmation | Ctrl-PgUp/PgDn when marks would be cleared |
| Destination conflict | Existing output; source/target metadata and overwrite/skip choices |
| Transfer failure | Retry, skip or cancel after a copy/move/extraction error |
| Notices | Errors, invalid input, missing matches and settings failures |
| File properties | Ctrl-A or supported Alt-Enter; DOS attributes and metadata |
| Recursive-search root | Alt-F7; starting directory |
| Recursive-search pattern | Alt-F7; filename pattern beneath that root |
| Recursive-search match | Enter opens the location, N continues, Esc stops |
| Text viewer | F3; also used for checked archive members |
| Hex viewer | Shift-F3, or F4 inside the text viewer |
| Viewer search | F7; text or hexadecimal pattern |
| Viewer byte offset | Ctrl-G; decimal or hexadecimal position |
| Counting files | Before copying or while calculating recursive selected-byte totals |
| Filename-search progress | Recursive enumeration; Escape cancels |
| Copy progress | Current-file and overall progress, transfer rate and ETA |
| Move/delete progress | Bytes, completed/skipped counts and cancellation |
| Archive destination | Ctrl-U on ZIP/ARJ; existing output directory |
| Archive extraction progress | Ctrl-U, or F5/Ctrl-U on archive members |
| Storage-test menu | Ctrl-T; quick, long, saved verification or cleanup |
| Storage target | Quick/long test; directory on the device being tested |
| Storage manifest/log directory | Quick/long test; evidence on another drive |
| Storage random seed | Quick/long test; reproducible 32-bit seed |
| Saved storage manifest | Verify/cleanup; path to a recorded plan |
| Storage-profile confirmation | Approve quick/long workload before creating test files |
| Missing-folder confirmation | Approve creation of a storage target or local evidence folder |
| Storage-cleanup confirmation | Approve removal of recorded test data |
| Storage dashboard | Running quick/long test, saved verification or cleanup |
| Storage result | Passed, failed or cancelled report; retained evidence and next steps |

## Shared graphics style

CGA, EGA and VGA use the same native frame function,
`graph_window_frame()` in [GRAPH.C](../src/GRAPH.C): black outline, light edge,
shadow, title strip, white title text and an icon. EGA/VGA use the blue title
strip; CGA uses its monochrome equivalent. Icons identify documents, folders,
drives, display settings or archives.

Help, About, viewers and copy/unpack progress paint native document bodies.
The smaller menus and prompts keep their established cell-based keyboard layout,
with native chrome painted after the body flush so cells cannot overwrite it.
The storage dashboard shares the frame while retaining its checklist, status
badges and incremental progress updates. Text mode keeps its character-based
presentation and controls.

`tests/modals.py` checks title strips, icons, repeated entry and restoration in
all display modes. Viewer, transfer and storage tests cover the other native
windows and nested prompts. Run `make test-video` for the complete UI checks.
