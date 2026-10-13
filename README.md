<p align="center">
  <img src="packaging/apps/harbour-qqmusic/icons/generated/172x172/harbour-qqmusic.png" alt="ZZ Music" width="144" height="144">
</p>

# ZZ Music

**ZZ Music** 是面向 Sailfish OS 的非官方 QQ 音乐原生客户端。应用使用 Qt 5、Sailfish Silica 和 Sailjail，针对手机触控、Cover 与系统媒体控制进行了适配。

当前公开版本为 **native-v1.0.0-27**，重点修复扫码登录竞态、同账号重新登录和 Sailfish 5.1 暂停进度归零问题。此版本为 AArch64 development prerelease，自动化及服务连通检查已通过，真实扫码与 Secrets 提交仍待实机确认。

## 功能

- 首页推荐、每日 30 首、猜你喜欢与推荐歌单。
- 发现分类、语种/流派筛选和独立排行榜页面。
- 单曲与歌单搜索，以及登录后的我的歌单。
- 单进程 Qt 5 二维码登录；会话仅在远程验证通过后写入 Sailfish Secrets。
- 播放队列、顺序循环、单曲循环和随机播放。
- Sailfish Cover 多个播放控制、透明歌词、封面自适应前景色和当前码率。
- MPRIS 系统媒体控制、后台播放和 Sailfish 5.1 兼容的暂停/恢复保护。
- 异步图片缓存与分阶段内容加载，降低页面切换和资源加载卡顿。

## 安装

从 [native-v1.0.0-27 Release](https://github.com/ZeroX-S/zzmusic/releases/tag/native-v1.0.0-27) 下载：

~~~text
zzmusic-1.0.0-27.bonito.dev.20261013t113645z.8ef344bc6f8b.aarch64.rpm
SHA-256: 923c3d70a7ea1d3afb10503984343b001051509c134e7d1f74d36ebe3f3e8214
~~~

在 AArch64 Sailfish OS 设备上安装：

~~~sh
devel-su zypper install --no-recommends \
  ./zzmusic-1.0.0-27.bonito.dev.20261013t113645z.8ef344bc6f8b.aarch64.rpm
~~~

安装包名称为 **zzmusic**，应用显示名称为 **ZZ Music**。当前版本使用 **/usr/bin/harbour-qqmusic** 和 **harbour-qqmusic.desktop** 作为兼容运行标识。

图标：[1024×1024 原图](packaging/apps/harbour-qqmusic/icons/harbour-qqmusic-master.png) · [172×172 PNG](packaging/apps/harbour-qqmusic/icons/generated/172x172/harbour-qqmusic.png)。Release 同时提供独立图标附件。

## 兼容性与验证

- 构建兼容目标：**SailfishOS-5.1.0.11-aarch64**，Qt **5.6.3**。
- 完整 SDK 回归：25 个 C++ 套件 **482/482**，QML **44/44**；最终会话隔离补丁另通过应用控制器 **34/34** 定向回归。
- Sailfish 目标库下真实服务的出码、MQTT 订阅、取消和重新出码连续 **3 轮通过**，未执行账号扫码或写入会话。
- 专用与汇总 RPM 校验通过。**本版尚未完成真实扫码、Secrets 提交及 5.1 手机暂停/恢复验收**；不沿用旧版的实机结果作为本版通过证据。
- 该 RPM 为 development prerelease；不同源码不会复用同一 NEVRA。

构建与测试方法见 [BUILDING.md](BUILDING.md)，本次发布说明见 [RELEASE_NOTES.md](RELEASE_NOTES.md)。

## 隐私与产品边界

- 不提供密码输入、Cookie 编辑、账号写操作、音乐下载或 DRM 绕过。
- 登录和播放均在应用现有 Qt 5 进程内完成，不启动 Qt 6、WebEngine、浏览器或登录 helper。
- Cookie、账号标识、二维码内容、完整 URL、原始响应和 Secrets payload 不进入公开日志或仓库。
- QQ Music 接口可能变化，功能可用性取决于地区、账号权限、网络和上游服务。

安全问题请按 [SECURITY.md](SECURITY.md) 私密报告。

## 上游与许可

QQ 服务行为参考 [Listen1 Chrome Extension](https://github.com/listen1/listen1_chrome_extension) 的 QQ provider，并以原生 C++ 请求和解析器重新实现。详细来源见 [UPSTREAM.md](packaging/apps/harbour-qqmusic/UPSTREAM.md)。

本项目采用 [MIT License](LICENSE)。Listen1 相关许可见 [LICENSE.listen1](packaging/apps/harbour-qqmusic/LICENSE.listen1)，Qt 运行时许可说明见 [QT-LICENSES.md](packaging/apps/harbour-qqmusic/QT-LICENSES.md)。

**ZZ Music** 是独立的非官方社区客户端，与腾讯或 QQ 音乐不存在隶属、授权、认可或赞助关系。QQ 音乐相关名称与标识归其权利人所有。
