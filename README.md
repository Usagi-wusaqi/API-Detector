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

## 构建

```bash
go build ./cmd/apidetect
```

## 基本用法

查看内置 Provider：

```bash
go run ./cmd/apidetect providers
```

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
  --method GET \
  --header "X-Test: 1" \
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
- `--concurrency`：并发数，默认 `100`
- `--timeout`：单请求超时，默认 `10s`
- `--format`：`text` 或 `json`
- `--export-valid`：导出有效 key 到文件
- `--url`：自定义端点 URL，仅 `custom` 使用
- `--method`：自定义 HTTP 方法，仅 `custom` 使用
- `--header`：自定义请求头，可重复传入，仅 `custom` 使用

## 输入规则

- 每行一个 key
- 空行会被忽略
- 以 `#` 开头的行会被视为注释
- 会自动去重，并保留首次出现顺序

## 发布

- `CI` 会在 Windows、Linux、macOS 上执行 `go test`、`go build` 和基础 smoke test
- 打 tag 后由 `GoReleaser` 打包并上传到 GitHub Releases
- 发行包为便携压缩包，包含二进制、`README.md`、`LICENSE`
- macOS 当前为未签名发行包，首次运行可能需要手动放行

## 仓库结构

```text
cmd/apidetect          CLI 入口
internal/core          检测执行流、解析、类型定义
internal/providers     Provider 适配器
internal/output        输出和导出逻辑
.github/workflows      CI 与发布流程
```

## License

[GNU GPL-3.0](LICENSE)
