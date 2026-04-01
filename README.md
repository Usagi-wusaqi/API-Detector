# API Detector v3.0.0

用于批量检测 API Key 可用性的命令行工具。

[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Go](https://img.shields.io/badge/Go-1.26+-00ADD8.svg)](https://go.dev/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)](https://github.com/Usagi-wusaqi/API-Detector)

## 功能

- 高并发批量检测
- 支持中断取消
- 支持文本输出和 JSON 输出
- 支持导出有效 key
- 默认脱敏输出，不在标准输出中暴露完整 key

## 支持的 Provider

- `openai` -> `https://api.openai.com/v1/models`
- `groq` -> `https://api.groq.com/openai/v1/models`
- `mistral` -> `https://api.mistral.ai/v1/models`
- `deepseek` -> `https://api.deepseek.com/models`
- `openrouter` -> `https://openrouter.ai/api/v1/models`
- `anthropic` -> `https://api.anthropic.com/v1/messages`
- `gemini` -> `https://generativelanguage.googleapis.com/v1beta/models`
- `custom` -> 自定义 Bearer 鉴权端点

常用别名：

- `claude` -> `anthropic`
- `google` -> `gemini`
- `or` -> `openrouter`

## 构建

```bash
go build ./cmd/apidetect
```

Windows 也可以直接运行：

```powershell
.\build.ps1
```

或：

```bat
build.bat
```

Linux / macOS 可以运行：

```bash
./build.sh
```

构建后会得到两个入口：

- `apidetect` / `apidetect.exe`：CLI
- `apidetect-gui` / `apidetect-gui.exe`：GUI 启动器

## 基本用法

启动本地 GUI：

```bash
go run ./cmd/apidetect-gui
```

如果已经有一个 GUI 实例在运行，新的启动会直接复用已有实例，不会再起第二个服务端。

如果不想自动打开浏览器：

```bash
go run ./cmd/apidetect gui --no-open
```

作为本地网页版服务运行：

```bash
go run ./cmd/apidetect web --listen 127.0.0.1:8787
```

查看内置 Provider：

```bash
go run ./cmd/apidetect providers
```

以 JSON 输出 Provider 列表：

```bash
go run ./cmd/apidetect providers --format json
```

查看版本：

```bash
go run ./cmd/apidetect version
```

发行版构建会注入版本号、提交号和构建时间。

从文件检测：

```bash
go run ./cmd/apidetect check --provider openai --input example_keys.txt
```

从标准输入检测并导出有效 key：

```bash
cat keys.txt | go run ./cmd/apidetect check --provider openai --export-valid valid_keys.txt
```

使用自定义端点：

```bash
go run ./cmd/apidetect check \
  --provider custom \
  --url https://example.com/v1/models \
  --method POST \
  --body '{"ping":true}' \
  --header "X-Test: 1" \
  --input keys.txt
```

使用自定义 `x-api-key` 鉴权：

```bash
go run ./cmd/apidetect check \
  --provider custom \
  --url https://example.com/check \
  --method POST \
  --auth-mode none \
  --header "x-api-key: {key}" \
  --body '{"key":"{key}"}' \
  --input keys.txt
```

输出 JSON：

```bash
go run ./cmd/apidetect check --provider gemini --input keys.txt --format json
```

## CLI 参数

`apidetect check` 支持：

- `--provider`：选择 provider
- `--input`：输入文件路径；省略时从 `stdin` 读取
- `--output`：将主报告写入文件，而不是输出到标准输出
- `--concurrency`：并发数，默认 `100`
- `--timeout`：单请求超时，默认 `10s`
- `--format`：`text` 或 `json`
- `--proxy-mode`：代理模式，支持 `env`、`direct`、`custom`
- `--proxy-url`：自定义代理地址，仅 `proxy-mode=custom` 使用
- `--export-valid`：导出有效 key 到文件
- `--export-invalid`：导出无效 key 到文件
- `--export-error`：导出错误结果对应的 key 到文件
- `--fail-on-invalid`：检测到无效 key 时返回非零退出码
- `--fail-on-error`：检测结果里存在错误时返回非零退出码
- `--quiet`：文本模式下不逐条输出结果，只保留最终汇总
- `--url`：自定义端点 URL，仅 `custom` 使用
- `--method`：自定义 HTTP 方法，仅 `custom` 使用
- `--auth-mode`：自定义认证模式，支持 `bearer` 和 `none`
- `--body`：自定义请求体，仅 `custom` 使用
- `--body-file`：从文件读取自定义请求体，仅 `custom` 使用
- `--header`：自定义请求头，可重复传入，仅 `custom` 使用

## 输入规则

- 每行一个 key
- 空行会被忽略
- 以 `#` 开头的行会被视为注释
- 会自动去重，并保留首次出现顺序

## 发布

- `CI` 会在 Windows、Linux、macOS 上执行 `go test`、`go build` 和基础 smoke test
- 打 tag 后由 `GoReleaser` 打包并上传到 GitHub Releases
- 也可以在 GitHub Actions 页面手动触发 `Release` workflow
- 发行包为便携压缩包，包含二进制、`README.md`、`LICENSE`
- macOS 当前为未签名发行包，首次运行可能需要手动放行

推荐发布方式：

- `dist/release/apidetect_v3.0.0_windows_amd64.zip`
- `dist/release/apidetect_v3.0.0_linux_amd64.tar.gz`
- `dist/release/apidetect_v3.0.0_darwin_amd64.tar.gz`
- `dist/release/apidetect_v3.0.0_darwin_arm64.tar.gz`
- `dist/release/checksums.txt`

发布步骤：

```bash
git tag v3.0.0
git push origin v3.0.0
```

## 退出码

- `0`：执行成功
- `2`：命令行参数错误
- `3`：启用 `--fail-on-invalid` 时检测到无效 key
- `4`：启用 `--fail-on-error` 时检测结果存在错误
- `130`：用户取消执行

## 仓库结构

```text
cmd/apidetect          CLI 入口
internal/core          检测执行流、解析、类型定义
internal/gui           本地 GUI / Web 服务
internal/providers     Provider 适配器
internal/output        输出和导出逻辑
.github/workflows      CI 与发布流程
```

## License

[GNU GPL-3.0](LICENSE)
