# Upstream provenance

The QQ provider behavior in this application was ported from Listen1 Chrome
Extension commit `3f24efa045125875a609dcb0f3f3f0be4edb8b37`:

- https://github.com/listen1/listen1_chrome_extension

The native implementation derives behavior from the provider methods
`qq_show_toplist`, `show_playlist`, `qq_toplist`, `qq_get_playlist`, `search`,
`bootstrap_track`, `lyric`, `get_playlist_filters`,
`get_user_created_playlist`, `get_user_favorite_playlist`,
`get_recommend_playlist`, and `get_user`.

The port is implemented as bounded native C++ requests and parsers. It does not
ship the upstream JavaScript application, HTML/CSS web interface, other music
providers, source maps, GitHub OAuth credentials, Last.fm credentials, or any
upstream login payload. Listen1's MIT license is reproduced in
`LICENSE.listen1`.
