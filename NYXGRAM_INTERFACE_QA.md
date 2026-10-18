# Interface and audio refinement — review build

## Changes

- Click the audio-bar title to open the native list for music or voice/round-video. Enter/Space also open it when the bar is focused. Ctrl-click retains navigation to the source message; another account's playback keeps source navigation rather than displaying a foreign playlist.
- The native panel chooses the current audio type, resets its subscriptions/animation when types change, scrolls to the current music/voice item, and permits a one-entry list. Saved music keeps its non-media section precondition. Existing music-hover behavior is retained.
- The voice list uses Telegram Desktop's native per-chat voice/round-video shared-media provider; it is not an editable global queue, and it does not implement Android's remaining-queue UI. Topic ordering and autoplay equivalence require native QA.
- Deleted-message copies have 78% painter opacity, with painter state restored. A single vector deletion marker replaces the duplicate glyph/overpainted text. Layout and timestamp hit zones reserve its width, with RTL-aware geometry and the configured marker color. Retention/encryption/exclusion/PIN behavior is not broadened or bypassed.
- Shorter profile heading and ID/DC explanation; technical values, copy, links and bookmarks remain available.
- Replaced the external Qt settings dialogs with seven native Telegram Settings::Section pages. The host owns navigation, scrolling, DPI and theme. History search and asynchronous PIN forms use native GenericBox layers. Removed the forced dark stylesheet. Existing preference keys and account scoping are retained.
- The explicit Nyx night action packages a new plain-text palette through the existing Qt resource alias. The native loader supports non-ZIP palettes; this intentionally keeps the user's current chat wallpaper. The old ZIP theme remains in source as a fallback. Incoming/outgoing selected bubbles now have distinct colors, primary buttons have dark readable labels, and conversation colors align with Android. Existing user themes are not force-switched.
- Fixed a garbled local-pins label.

## Automated checks run locally

- `python3 tools/check_nyx_port.py` — 34 existing source-integration checks.
- `python3 tools/test_nyx_interface.py` — 43 interface wiring checks; 15 changed C++ files parsed with tree-sitter; palette reference/Qt resource checks; six foreground/background text pairs meet 4.5:1 contrast by calculation.
- `git diff --check`

Dependencies for the new checks: `tree-sitter==0.26.0`, `tree-sitter-cpp==0.23.4`. A separate read-only PR workflow runs the checks, without build/signing secrets or release publication.

These are source/syntax checks, NOT C++ compilation or runtime/UI tests. `info_profile_actions.cpp` has an existing preprocessor/parser ambiguity; only its changed label is checked separately, not reported as a clean full-file parse.

## Not yet verified

The editor environment is Linux without the project's Windows/MSVC/Qt dependency toolchain. Previous build 13 was compiled and published by Windows CI; user screenshots exposed the external-window design flaw and unsupported deleted-media notice. This native-settings correction requires its own successful Windows CI run. Manual Windows UI acceptance remains outstanding.

## Windows acceptance checklist

- [ ] Build with the existing Windows x64 workflow/toolchain (including submodules and Qt resources).
- [ ] Music: mouse click, hover shortcut and keyboard activation; native playback/seek/order controls; a one-track list; saved profile music.
- [ ] Voice/round video: list opens, current entry scrolls into view, selecting entries plays them; loading/empty lists, auto-next, account/topic/migrated-chat boundaries.
- [ ] Switch music/voice/round-video and accounts while the list is open or animating; no stale rows, subscriptions or crashes.
- [ ] Ctrl-click and different-account playback still navigate to the source rather than exposing a wrong-account list.
- [ ] Deleted text/media/album messages at narrow widths and 100/125/150/200% DPI: exactly one marker, no overlap with timestamp/views/replies/pinned/send state; RTL; correct colors on light/dark themes.
- [ ] Archive PIN relock, preservation toggle, retention/clear, restored local copies, secret/TTL/view-once exclusions remain intact.
- [ ] Settings → Nyxgram stays in the same Telegram settings host; all seven pages support back navigation, minimum width, 100/125/150/200% DPI, light/dark/custom themes and keyboard controls. No additional Nyxgram settings taskbar window.
- [ ] PIN entry/change and archive search are native Telegram layers. Switch account, lock app and background during each delayed action; no cross-account archive exposure or mutations.
- [ ] Supported message followed by unsupported-media update then deletion retains the earlier renderable snapshot. Unsupported copies retain their available caption; absent caption gets an honest local unavailable-media label. Ordinary live unsupported messages keep Telegram's warning.
- [ ] Change to TTL/view-once after a supported snapshot: purge still wins over unsupported fallback. Five invalid PIN attempts throttle for 60 seconds; valid PIN opens only the current account for five minutes.
- [ ] Explicit Nyx theme Apply/Restore loads the packaged plain palette and preserves wallpaper; selected text/bubbles, links, timestamps, buttons and service text are readable. Verify on the rendered UI, not just contrast calculations.

## Native Windows build follow-up

The first full Windows CI run found three inherited compilation defects: Qt 6-only QList::removeIf on the project's Qt 5.15.17, an incomplete DocumentData type in the message renderer, and three click-filter-signature lambdas passed to FlatLabel::overrideLinkClickHandler. The follow-up uses standard erase/remove_if, includes data_document.h explicitly, and uses the pinned lib_ui no-argument callback overload. The integration checks cover these regressions. A successful full native rebuild is still required before publishing the Windows package.

The second Windows run passed nyx_compile_check, then exposed a renderer error outside that target: myrtlrect is an RpWidget member and is unavailable in the free deletion-marker helper. The helper now uses style::rtlrect. The native preflight has been expanded to include the deletion renderer and all newly changed player/list/mainwidget integration units, so these compile errors are caught before the full executable build.

## Native settings correction

Source checks now cover native Section factories/routing, no standalone main/features/history dialogs, no forced stylesheet, asynchronous native PIN layers, salt validation/retry throttle, guarded delayed actions, and TTL-before-unsupported snapshot handling. These checks do not replace MSVC compilation or visual inspection on Windows.
