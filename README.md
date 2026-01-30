# ⚡ API Detector v2.0.0

跨平台高性能 AI 大模型 API Key 检测器

[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![.NET](https://img.shields.io/badge/.NET-8.0-512BD4.svg)](https://dotnet.microsoft.com/)
[![Avalonia](https://img.shields.io/badge/Avalonia-11.x-8B5CF6.svg)](https://avaloniaui.net/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)](https://github.com/Usagi-wusaqi/API-Detector)

> 🚀 **C# + Avalonia 重构版** — 跨平台 GUI，单文件部署，双击即用！

## ✨ 特性

- 🌍 **跨平台支持** — Windows / macOS / Linux
- ⚡ **高并发检测** — 支持 1000+ 并发连接
- 🎨 **现代化 GUI** — Fluent 暗色主题
- 📦 **单文件部署** — 无需安装运行时
- 🔌 **多平台 API** — OpenAI / Claude / Gemini / DeepSeek 等
- 📊 **实时统计** — 进度、速度、结果一目了然
- 📁 **批量导入导出** — 支持文件加载和结果导出

## 🚀 快速开始

### 使用方法（Windows）

1. 下载源码（[Download ZIP](https://github.com/Usagi-wusaqi/API-Detector/archive/refs/heads/main.zip)）
2. 解压后在根目录找到 `ApiDetector.exe`
3. **双击 `ApiDetector.exe`** 即可运行，无需安装任何东西

### 从源码编译（维护者）

如需修改代码并提交，请先本地编译更新根目录产物：

- Windows：双击 `build.bat`
- 或手动执行：
```bash
# 安装 .NET 8.0 SDK 后
dotnet publish src/ApiDetector.csproj -c Release -r win-x64 -o . --self-contained true
```

## 📖 使用说明

1. **选择 API 预设** — 下拉选择 OpenAI / Claude / Gemini 等，或自定义端点
2. **输入 API Keys** — 在左侧文本框输入，每行一个；或点击"加载文件"批量导入
3. **调整参数** — 设置并发数（默认 100）和超时时间（默认 10s）
4. **开始检测** — 点击"开始检测"按钮
5. **查看结果** — 实时显示检测进度和结果
6. **导出有效 Keys** — 点击"导出有效"保存到文件

## 🔌 支持的 API 平台

| 平台 | 预设名称 | 测试端点 |
|------|----------|----------|
| OpenAI | OpenAI | `/v1/models` |
| Anthropic | Anthropic Claude | `/v1/messages` |
| Google | Google Gemini | `/v1beta/models` |
| Groq | Groq | `/openai/v1/models` |
| Mistral | Mistral | `/v1/models` |
| DeepSeek | DeepSeek | `/v1/models` |
| OpenRouter | OpenRouter | `/api/v1/models` |

也可以选择"自定义"输入任意 API 端点。

## 📁 项目结构

```
API-Detector/
├── ApiDetector.exe         # 可执行文件 (双击运行)
├── *.dll                   # 运行时依赖
├── build.bat               # 编译脚本
├── README.md               # 文档
├── LICENSE                 # 许可证
└── src/                    # 源码目录
    ├── ApiDetector.csproj  # 项目配置
    ├── Program.cs          # 入口
    ├── App.axaml(.cs)      # 应用定义
    ├── Models/             # 数据模型
    ├── Services/           # 检测服务
    ├── ViewModels/         # MVVM ViewModel
    └── Views/              # 主界面
```

## 📊 技术栈

- **语言**: C# 12 / .NET 8.0
- **UI 框架**: Avalonia 11.x (跨平台)
- **架构模式**: MVVM (CommunityToolkit.Mvvm)
- **HTTP 客户端**: System.Net.Http
- **主题**: Fluent Dark

## 📄 License

[GNU GPL-3.0](LICENSE)
