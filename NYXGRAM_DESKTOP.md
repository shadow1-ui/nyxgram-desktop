# Nyxgram Desktop 0.1 — Windows x64

Independent GPL client based on Telegram Desktop 6.1.1, imported master base `86262333a457f62709726ee4ab9c48fa58824da4`.

## This first version

Nyxgram name/icon/executable, distinct AppId/Qt identity/IPC GUID/notification identity, no migration from official Telegram's data directory. Portable ZIP containing Nyxgram.exe plus necessary emitted runtime files and license notices. No installer or Authenticode certificate in this first version. Windows may show an unsigned-application warning; do not turn off Windows security settings.

Official Telegram auto-update and crash-upload mechanisms disabled at build time. Automatic tg:// association registration disabled; existing default handler is untouched on ordinary launch. Manual registration remains a native user choice. Do not place this client in an existing Telegram folder or copy official tdata into it. Extract to a new Nyxgram folder and sign in separately. Use the local passcode option if desired.

## Not yet ported

Android Nyxgram settings, ghost mode, ID/DC, local deleted/edit archive, custom appearance and Android's GIF/sticker confirm-preview are NOT implemented in Desktop 0.1. This is the isolated desktop foundation, not feature parity. Official Telegram Desktop features continue to work subject to testing.

## Build

GitHub Actions Windows-2022 / Visual Studio 2022 / SDK 10.0.26100.0 / x64 Release, upstream library preparation with ThirdParty/Libraries caches. API_ID and API_HASH must be provided as repository Actions secrets from your own Telegram API application; no test credentials are used. Without them the workflow only prepares/caches dependencies and produces no executable. Dispatch `dependencies_only=false` after adding the secrets; true explicitly warms caches.

Other imported upstream bots/platform workflows are disabled, not deleted. There is one Windows x64 job. Dependencies are cached before application compilation, so a later application error does not discard successful preparation. Public repo runs do not guarantee a short first build.

## Testing status

Local source/configuration/icon checks are not Windows runtime tests. CI compilation and device QA remain required: login, messaging, stickers/GIF, calls, local passcode, tray/toast, normal Telegram running beside Nyxgram, separate data directory, tg:// unchanged and correct executable. No desktop installer/updater or Android features are claimed by a successful dependency job.

Sources: https://github.com/shadow1-ui/nyxgram-desktop ; original authors/license retained in LICENSE and LEGAL.
