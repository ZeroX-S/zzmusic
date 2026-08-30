# QQ Music Native UI and UX Design

## Silica Foundation

- Every screen is a Sailfish `Page`, `CoverBackground`, or `DockedPanel` and
  uses Silica navigation, flicking, highlight, remorse, and ambience behavior.
- Layout dimensions, text sizes, colors, opacity, margins, and touch targets
  come from `Theme`; the design does not introduce a second visual system.
- In-page primary commands stay visible. Root navigation and refresh use the
  native `PullDownMenu` pattern. Lists use `SilicaListView`, long content uses
  `SilicaFlickable`, and destructive actions use `RemorsePopup`.
- Compact headers deliberately use Silica theme metrics at
  `Theme.itemSizeMedium`. They preserve native back-swipe navigation and avoid
  adding Android-style app bars, floating actions, or bottom navigation.
- Highlight color is reserved for the active state and current playback.
  Ambience surfaces remain quiet so artwork and readable metadata lead the
  composition.

## Product Structure

- Home is a focused discovery page. My Music, Search, Settings, and Refresh are
  grouped in its pull-down menu. A single recommendation flow uses a two-column
  portrait grid and a three-column landscape grid.
- My Music owns account playlists and the QR sign-in presentation. Sign-in is
  a state of this page, not a browser or secondary process.
- Search keeps Tracks and Playlists as an explicit segmented view. Results
  retain their query and pagination state while switching views.
- Playlist detail presents artwork, title, owner/count metadata, one Play All
  command, and a scan-friendly track list.
- Player keeps Cover and Lyrics as explicit peer views. Transport, progress,
  metadata, and queue access remain in fixed positions while the view changes.
- Queue is a bottom docked panel over Player. It preserves the current track,
  supports direct selection/removal, and protects Clear Queue with Remorse.
- Settings groups Playback, Cache, Account, and About. Binary settings use a
  switch, bounded options use a selector, and destructive actions use Remorse.
- The Sailfish Cover shows artwork, playback identity, progress, Play/Pause,
  and Next without launching another process.

## Page Blueprints

### Home

- A compact identity header leads directly into one recommendation flow.
- My Music, Search, Settings, and Refresh live in the pull-down menu. There are
  no duplicate icon shortcuts or bottom navigation.
- Language, genre, category, public-playlist, chart, and recent-queue sources
  are not rendered as separate Home sections. Portrait recommendations use two
  columns; landscape recommendations use three.
- Cached recommendations remain interactive while a narrow offline state
  explains freshness.

### My Music and QR Sign-in

- The signed-in state leads with avatar, account display name, and a quiet
  status line, then separates liked, created, and collected playlists.
- The signed-out state keeps the same page hierarchy and centers one bounded
  QR task. Preparing, QR ready, scanned, validating, timeout, cancelled, and
  failed states never shift the page title or move the primary action outside
  the viewport.
- QR login stays in the app process and uses the existing Secrets-backed
  session contract. No browser, Cookie editor, or second process is added.

### Search

- A native `SearchField` follows the compact title. Tracks and Playlists are
  peer segmented modes with a shared query and visible active indicator.
- Track results favor title, artist/album, and row actions. Playlist results
  use fixed artwork geometry, title, track count, and author.
- Debounced typing reduces unnecessary requests; explicit Enter remains
  available. Loading, empty, retry, offline, and pagination states preserve
  the query and selected mode.

### Playlist and Chart Detail

- Fixed-size asynchronous artwork anchors the header. The title may use two
  lines; count, owner, and chart period remain subordinate.
- Play All is a visible primary command and also remains available in the
  pull-down menu. Track rows retain Sailfish context actions.
- The scrolling track list owns the header, so content moves as one coherent
  Silica surface while the bottom player remains docked.

### Player: Cover

- Cover and Lyrics are explicit peer tabs and are also reachable by horizontal
  swipe. The selected tab, swipe index, and indicator are one state.
- Artwork is capped near 38 percent of portrait height. It decodes
  asynchronously inside fixed bounds, with placeholder, busy, ready, and error
  presentation that never moves metadata or controls.
- Title, artist, album, progress, time, transport, repeat, and queue access have
  stable vertical zones. Landscape uses a dedicated two-column composition.

### Player: Lyrics

- The current line is centered near the visual focus and uses highlight color,
  higher opacity, and stronger weight. Neighboring lines remain readable
  without competing.
- Tapping a line seeks to its timestamp. Manual scrolling pauses automatic
  following for four seconds, then restores the current-line focus.
- Loading, no-lyrics, and failure states use the same bounded lyric region and
  cannot overlap transport controls.

### Queue

- A modal bottom `DockedPanel` keeps the current track identity in its header
  and uses a `SilicaListView` for direct selection.
- The active track has a narrow highlight marker. Remove is a direct icon
  command; Clear Queue is protected by `RemorsePopup`.
- Dismissing the panel returns to the same Player state and never restarts the
  playback process.

### Settings

- Playback, Cache, Account, and About are visually separated by compact
  Silica section headings instead of nested cards.
- Resume is a `TextSwitch`; cache limit is a `ComboBox`; cache and session
  removal use explicit actions with remorse. Sign out appears only when a
  session exists.
- Version, upstream, notices, and licenses stay informational and subordinate
  to operational settings.

### Persistent Bottom Player

- Home, My Music, Search, Playlist, and Settings reserve layout space for the
  player whenever a current track exists; it never overlays list content.
- Fixed artwork, two text lines, top-edge progress, Play/Pause, and Next form a
  stable Silica-sized control. Tapping the non-button body opens Player.

### Sailfish Cover

- The Cover uses artwork as the primary signal, then title, artist, and a
  compact progress line. Idle state shows the product identity without stale
  transport state.
- Native Cover actions expose Play/Pause and Next. Foreground and Cover cycles
  retain one application process and playback session.

### Loading, Empty, Offline, and Error

- Blocking loading is used only before first content. Once cached content
  exists, an offline message is non-blocking and refresh remains available.
- Empty and error states preserve the page title, back navigation, and retry
  position. All state text wraps inside page margins at 540x960 and 1080x2160.

## Visual Hierarchy

- Page title: `Theme.fontSizeLarge`, one line, left aligned.
- Section title and current song title: `Theme.fontSizeMedium`, DemiBold.
- Row/card title: `Theme.fontSizeSmall`, at most two lines for playlists and
  one faded line for tracks.
- Metadata: `Theme.fontSizeExtraSmall`, secondary color, one line.
- Portrait playlist grids always use two columns. Display DPI must not be used
  as a proxy for orientation.
- Artwork geometry is stable before network content arrives. Images decode
  asynchronously into fixed bounds and never resize surrounding controls.
- Highlight color indicates selection, playback, progress, and the active
  lyric only. Large surfaces remain neutral ambience colors.

## Interaction Contract

- Every icon-only command uses a platform-recognizable icon and at least a
  `Theme.itemSizeSmall` touch target.
- The persistent bottom player is visible whenever a current track exists and
  never covers scrollable content. Tapping its body opens Player; Play/Pause
  and Next remain direct actions.
- Player view tabs and swipe gestures update the same Cover/Lyrics index.
- A lyric tap seeks to that line and resumes automatic following. Manual lyric
  scrolling pauses follow for four seconds before returning to the current
  line.
- Pull-down menus remain supplementary for refresh and source actions; primary
  navigation must be visible on screen.
- Loading blocks only when no content is available. Cached content remains
  usable with a non-blocking offline banner. Empty and error states keep page
  titles and retry navigation stable.
- QR images are fixed-size, synchronous memory images. Account/session data is
  never rendered in diagnostic artifacts or logs.

## Page Acceptance

- Home, My Music, Search, Playlist, Settings, Player, Lyrics, Queue, and Cover
  fit 540x960 and the real 1080x2160 portrait surface without overlap.
- Landscape Player uses a dedicated two-column layout rather than scaling the
  portrait scene.
- The same Qt 5 process and PID survive Player/Lyrics/Queue and Cover cycles.
- Login behavior, Sailfish Secrets persistence, network policy, playback
  resolution, and MPRIS behavior are unchanged by the presentation redesign.
