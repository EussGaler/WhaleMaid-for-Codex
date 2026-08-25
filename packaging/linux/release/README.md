# WhaleMaid-for-Codex Ubuntu x64 v1.3.3

此目录是面向 Ubuntu 22.04 x86_64 的完整 Release。它内置 Qt 6、Live2D Cubism Core、角色资源和运行脚本，不需要安装 Qt、Cubism SDK、CMake 或编译器。

## 系统要求

- Ubuntu 22.04 Desktop x86_64；
- X11 会话，或 Wayland 会话中的 XWayland；
- 可正常使用的 OpenGL 显卡驱动；
- 已安装并使用 Codex CLI、Codex 桌面端或 VS Code 中的 Codex，会话保存在 `~/.codex/sessions`。

不支持纯原生 Wayland 模式。Ubuntu Desktop 自带的系统图形库和显卡驱动属于操作系统前置环境，不随本包替换。

## 三个快捷入口

- `安装WhaleMaid.sh`：安装到当前用户目录、创建应用菜单入口、首次安装默认启用开机启动，并启动桌宠；
- `启动WhaleMaid.sh`：启动或唤醒已安装的桌宠；
- `卸载WhaleMaid.sh`：移除程序、应用菜单入口、自启动和 WhaleMaid 本地状态。

先完整解压 `.tar.gz`，然后在文件管理器中双击相应脚本并选择“运行”。如果文件管理器没有提供运行选项，可在此目录执行：

```bash
./安装WhaleMaid.sh
./启动WhaleMaid.sh
./卸载WhaleMaid.sh
```

若从不保留可执行权限的压缩格式解压，请先执行：

```bash
chmod +x ./*.sh ./scripts/*.sh
```

默认安装目录为 `~/.local/share/WhaleMaid`，日志目录为 `~/.local/state/WhaleMaid/logs`。安装后可以移动或删除解压目录；启动和卸载入口也会出现在桌面环境的应用菜单中。

WhaleMaid 直接读取本机 `~/.codex/sessions` 中最近 30 个日历日的候选会话，不上传提示词、回复或项目文件。

许可信息见 `LICENSE`、`NOTICE.md`、`ASSETS-LICENSE.md` 和 `licenses/`。
