# WhaleMaid-for-Codex

WhaleMaid-for-Codex 是面向 Windows Codex 桌面端和 VS Code 插件的 Live2D 桌宠。角色会自动呼吸、眨眼并跟随鼠标视线，同时显示 Codex 当前的思考、工作、批准和任务结果状态。

本项目公开源码并允许非商业学习、修改和二次开发，但不允许未经授权的商业使用。它不是采用 OSI 认可许可证的开源软件。女仆鲸鱼人设来自 bilibili: [@ZipZipPipe](https://space.bilibili.com/4168597)。感谢 [@XYDSYZ](https://github.com/XYDSYZ) 的测试与修改意见。

~~至于为什么要让大肥鱼夺舍Codex，本来想做dsh的，但是涨价后完全用不起了！~~

## 功能

- 原生 Live2D 模型渲染与物理效果；
- 眼球、头部和身体跟随鼠标；
- 自动呼吸与自然眨眼；
- 显示思考中、工作中、请求批准、任务完成和任务失败；
- 云朵状状态卡可显示在角色上方或左侧，随角色同比缩放且不会缩放或闪烁人物；
- 支持拖动、15%–200% 滑块缩放、锁定桌宠和清除提示；
- 自动记住上次退出时的窗口大小、位置、锁定状态和状态卡显示位置；
- 支持可关闭的开机启动、单实例运行和安全卸载；
- 本地读取 Codex 状态，但不会上传提示词、回复或文件内容。

### 效果预览

<img width="325" height="395" alt="演示图片" src="https://github.com/user-attachments/assets/f84eb989-fe84-4540-a71b-729bf3262278" />

https://github.com/user-attachments/assets/e47c77d5-10ef-469a-a449-9d0ef0f2bcd4

## 系统要求

- Windows 10 或 Windows 11，x64；
- 已安装并能正常使用 Codex 桌面应用或 VS Code 插件；
- Windows PowerShell；
- 可用的 OpenGL 显卡驱动。

Windows Release 已包含 Qt、Microsoft Visual C++ Runtime、Live2D 运行资源和 WhaleMaid 程序。普通用户无需另行安装开发环境。

## 安装

1. 从 GitHub Releases 下载 `WhaleMaid-for-Codex-Windows-x64-vX.Y.Z.zip`。
2. 将 ZIP 完整解压到普通文件夹。不要在压缩包预览窗口中直接运行。
3. 双击 **`安装WhaleMaid.cmd`**。
4. 等待窗口显示 `SUCCESS - WhaleMaid installation completed.`。
5. 阅读结果后，点击窗口右上角 `×` 手动关闭。
6. 回到 Codex 正常使用，无需重启 Codex 或创建专用对话。

安装程序会：

- 将完整运行程序复制到当前用户的本地程序目录；
- 将 WhaleMaid 状态 Hooks 合并到现有 Codex 配置，并保留其他 Hooks；
- 首次安装默认开启开机启动，更新时保留用户已有选择；
- 启动桌宠。

默认安装位置：

```text
C:\Users\你的用户名\AppData\Local\Programs\WhaleMaid\app
```

安装日志：

```text
C:\Users\你的用户名\AppData\Local\WhaleMaid\logs\installer.log
```

## 安装包与已安装程序

解压后的文件夹是安装介质；`AppData\Local\Programs\WhaleMaid` 中的是正式运行副本。两处包含重复的 `app` 文件是正常现象。

Codex Hooks、开机启动和日常运行均指向正式安装目录。因此移动或删除解压目录不会影响已经安装的桌宠，但会同时删除启动和卸载 CMD；需要时可以重新下载 Release 包。

## 使用

安装后 WhaleMaid 会随 Windows 登录自动启动。手动退出后，可双击 Release 包中的 **`启动WhaleMaid.cmd`** 重新启动。重复启动不会产生多个桌宠实例。

右键角色可以：

- 使用 15%–200% 滑块调整窗口大小；
- 在“显示位置”中选择状态卡显示在角色“上方”或“左侧”；
- 锁定或解锁桌宠；
- 清除全部状态提示；
- 查看关于信息；
- 退出 WhaleMaid。

选择右键菜单中的 **“关于”**，可以使用 **“开机时自动启动 WhaleMaid”** 复选框。修改会立即生效，安装新版本时会保留用户的选择。

空闲时角色头上不显示状态卡。普通状态会在后续状态出现后淡出；“请求批准”“任务完成”和“任务失败”需要点击卡片右上角 `×`，或使用右键菜单中的“清除提示”。

每次主任务只会显示一次“任务完成”或“任务失败”。Codex 内部用于审批检查的 guardian 子任务不会生成完成提示；每次实际出现的权限确认界面都会显示“请求批准”。

## 更新

1. 下载并完整解压新版本；
2. 双击新版 `安装WhaleMaid.cmd`；
3. 安装程序会关闭旧进程、替换运行文件并更新 WhaleMaid 自己的 Hooks；
4. 其他 Codex Hooks 和设置会被保留。

## 卸载

1. 双击 **`卸载WhaleMaid.cmd`**；
2. 按 `Y` 确认；
3. 等待窗口显示卸载成功；
4. 点击窗口右上角 `×` 手动关闭。

卸载程序只移除 WhaleMaid 程序、开机启动项和 WhaleMaid 自己的 Hooks。修改 Hooks 前会生成带时间戳的配置备份。

## 隐私

- WhaleMaid 与状态桥接仅在本机通信；
- 程序只处理任务生命周期、状态类型和任务标识；
- 不会将提示词、回复、工具参数、密钥或文件内容发送到网络；
- 安装和卸载会保留用户原有的其他 Codex Hooks。

## 源码构建

普通用户不需要执行本节。

构建环境：

- Visual Studio 2022，包含“使用 C++ 的桌面开发”；
- CMake 3.24 或更高版本；
- Qt 6.7.x MSVC 64-bit，包含 Widgets、OpenGL、Network 和 Test；
- Live2D Cubism SDK for Native 5-r.5；
- 已获授权的 WhaleMaid Live2D 模型资源。

```powershell
cmake -S .\src\desktop-app -B .\build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="D:\Qt\6.7.2\msvc2019_64" `
  -DWHALE_LIVE2D_SDK_ROOT="D:\SDK\CubismSdkForNative-5-r.5"

cmake --build .\build
ctest --test-dir .\build --output-on-failure
```

源码仓库不包含 Qt、Live2D Cubism SDK/Core 或未确认可再分发的角色资源。Windows Release 则面向普通用户提供已经部署的运行包。

## 项目结构

源码仓库：

```text
src\desktop-app\          C++、Qt 与 Live2D 集成源码
packaging\windows\        Windows 安装、启动和卸载脚本
README.md                  项目说明
LICENSE                    许可证
```

GitHub Release 压缩包：

```text
app\                       已部署的 Windows 运行程序
scripts\                   PowerShell 安装脚本
安装WhaleMaid.cmd           安装入口
启动WhaleMaid.cmd           启动入口
卸载WhaleMaid.cmd           卸载入口
README.md
LICENSE
```

## 许可

WhaleMaid 采用分层授权：

- 原创程序代码采用 PolyForm Noncommercial License 1.0.0，允许非商业使用、修改和分发；
- Live2D 角色、纹理和美术素材采用 CC BY-NC-SA 4.0，允许署名的非商业二次创作，并要求以相同方式共享；
- Qt、Live2D Cubism SDK/Core、Microsoft Runtime 等第三方组件继续遵循各自许可证。

“商业使用”包括但不限于销售软件或衍生版本、收费分发、集成到收费产品或服务，以及以预期商业应用为目的的使用。
