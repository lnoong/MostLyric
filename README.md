# MostLyric

MostLyric 是一个 Windows 桌面歌词显示工具。项目由一个控制面板程序和一个 Hook DLL 组成，可将手动文本或 QQ 音乐本地歌词显示到任务栏区域，并支持字体、颜色、尺寸、位置和歌词延迟调整。

![MostLyric Demo](demo.png)

## 功能

- 任务栏歌词显示
- 手动文本显示
- QQ 音乐本地歌词读取
- 歌词进度高亮与时间偏移调整
- 字体、字号、未唱/已唱颜色配置
- 中英文界面切换
- 实时预览、应用和停止

## 项目结构

```text
.
├── MostLyric.sln              # Visual Studio 解决方案
├── MostLyric/                 # 控制面板程序
├── MostLyricHook/             # 注入到 Explorer 的 Hook DLL
├── config/                    # 主程序和 Hook 共享配置结构
├── vendor/imgui/              # Dear ImGui 依赖
└── logo.png                   # 项目图片
```

## 环境要求

- Windows 10 或更高版本
- Visual Studio 2022
- MSVC v143 工具集
- Windows 10 SDK

## 构建

1. 使用 Visual Studio 打开 `MostLyric.sln`。
2. 选择 `Release | x64` 或 `Debug | x64` 配置。
3. 构建整个解决方案。
4. 输出文件位于：

```text
build/<Configuration>/<Platform>/
```

例如：

```text
build/Release/x64/MostLyric.exe
build/Release/x64/MostLyricHook.dll
```

也可以在 Developer PowerShell 中使用 MSBuild：

```powershell
msbuild MostLyric.sln /p:Configuration=Release /p:Platform=x64
```

## 使用

1. 构建 `MostLyric.exe` 和 `MostLyricHook.dll`。
2. 确保两个文件位于同一个输出目录。
3. 运行 `MostLyric.exe`。
4. 在界面中选择歌词来源、字体、颜色和显示区域。
5. 点击“应用”开始显示歌词，点击“停止”结束显示。

QQ 音乐本地歌词模式需要在界面中填写 QQ 音乐歌词缓存目录。若歌词无法显示，请先确认目标歌曲已有本地歌词文件。

## 配置与日志

- 配置会写入当前用户目录下的 MostLyric 配置文件。
- Hook 运行日志会写入桌面上的 `MostLyric.log`。

## 依赖

- [Dear ImGui](https://github.com/ocornut/imgui)
- Windows API、Direct2D、DirectWrite、DirectComposition、Direct3D 11

## 许可

本项目使用 MIT License 开源，详见 `LICENSE`。
