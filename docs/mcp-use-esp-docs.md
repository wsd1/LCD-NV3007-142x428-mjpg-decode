
使用pi-agent原生不支持mcp服务，还需去安装插件。但是使用mcp-use这个工具就简单了。下面是具体用法。

## 安装 与 连接

```
npm install -g @mcp-use/cli
npm install -g @mcp-use/client

# 连接远端ESP文档服务
mcp-use client connect espDocs https://mcp.espressif.com/docs

# 连接远端组件文档服务
mcp-use client connect espComponents https://components.espressif.com/mcp

```

## 列出当前mcp连接

Agent注意！
Agent注意！
Agent注意！

请优先使用该命令去获取已经建立的mcp连接。

	mcp-use client list

返回：

```
espDocs	https://mcp.espressif.com/docs
espComponents	https://components.espressif.com/mcp
```

## 列出mcp连接提供的tools

	mcp-use client espDocs tools list

```
search_espressif_sources - Perform semantic retrieval against Espressif documentation.

Returns relevant chunks from ingested documentation sources in descending
order of relevance, plus an interaction_id for give_feedback.

When to use:
    - Find documentation about Espressif products,
      ESP-IDF, and related technologies
    - Prefer this over asking questions directly — returns
      raw documentation chunks you can reason over

How to use:
    - Provide one focused semantic query
      (e.g., "WiFi configuration in ESP-IDF"); avoid many parallel searches
    - Set language to "cn" for Chinese documentation, or "en" (default) for English
    - Respect rate limits; back off if limited
    - After results, call give_feedback with the returned interaction_id

Example:
    search_espressif_sources("How to configure WiFi in ESP-IDF?")
    search_espressif_sources("如何在 ESP-IDF 中配置 WiFi？", language="cn")

Returns:
    interaction_id, language, and ranked documentation chunks with source URLs.

give_feedback - Rate a prior search_espressif_sources call.

Pass the interaction_id returned by search. Stars: 1–2 negative,
3–5 positive (3 still counts as positive). Prefer a short comment
explaining why — useful hits, wrong or outdated docs, missing topic,
or empty/irrelevant results. Comments help improve the docs and retrieval.
Only the caller who ran the search can rate it.

Example:
    give_feedback(interaction_id, stars=4, comment="Clear WiFi STA config chunks")
    give_feedback(interaction_id, stars=1, comment="No ESP32-C6 BLE results")
```


## 输出某个tool的描述

	mcp-use client espDocs tools describe search_espressif_sources

```
{
  "name": "search_espressif_sources",
  "description": "Perform semantic retrieval against Espressif documentation.\n\nReturns relevant chunks from ingested documentation sources in descending\norder of relevance, plus an interaction_id for give_feedback.\n\nWhen to use:\n    - Find documentation about Espressif products,\n      ESP-IDF, and related technologies\n    - Prefer this over asking questions directly — returns\n      raw documentation chunks you can reason over\n\nHow to use:\n    - Provide one focused semantic query\n      (e.g., \"WiFi configuration in ESP-IDF\"); avoid many parallel searches\n    - Set language to \"cn\" for Chinese documentation, or \"en\" (default) for English\n    - Respect rate limits; back off if limited\n    - After results, call give_feedback with the returned interaction_id\n\nExample:\n    search_espressif_sources(\"How to configure WiFi in ESP-IDF?\")\n    search_espressif_sources(\"如何在 ESP-IDF 中配置 WiFi？\", language=\"cn\")\n\nReturns:\n    interaction_id, language, and ranked documentation chunks with source URLs.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "query": {
        "type": "string"
      },
      "language": {
        "default": "en",
        "enum": [
          "en",
          "cn"
        ],
        "type": "string"
      }
    },
    "required": [
      "query"
    ],
    "additionalProperties": false
  },
  "outputSchema": {
    "type": "object",
    "additionalProperties": true
  },
  "annotations": {
    "readOnlyHint": true,
    "destructiveHint": false,
    "idempotentHint": true,
    "openWorldHint": true
  },
  "_meta": {
    "fastmcp": {
      "tags": []
    }
  }
}
```


## 调用Tool

mcp-use client espDocs tools call search_espressif_sources query="如何用ESP32S3驱动WS2812 8x8灯板" language="cn" --json



## ESP官方的mcp服务器列表

来源于： https://mcp.espressif.com/


### Espressif Documentation

搜索乐鑫技术文档，获取产品相关问题的解答。
https://mcp.espressif.com/docs

### ESP Component Registry

从 ESP 组件注册表中搜索组件和示例。

https://components.espressif.com/mcp

### Espressif Engineering

将乐鑫问题排查与解决流程接入您的 AI 助手。

https://ts-mcp.espressif.com/mcp

### Rainmaker

与乐鑫 Rainmaker 云服务进行交互。

https://mcp.rainmaker.espressif.com/api/mcp


### ESP-VISION

提供 ESP-VISION 与 MicroPython 的权威 API 及芯片可用性，用于摄像头与端侧 AI 开发。
https://mcp.vision.espressif.com

