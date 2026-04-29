// --------------------------------------------------------------------------------------------------------------------
// <copyright company="Microsoft">
//   Copyright (c) Microsoft. All rights reserved.
// </copyright>
// --------------------------------------------------------------------------------------------------------------------

#pragma warning disable OPENAI001 // OpenAI Responses APIs are experimental in the official OpenAI package.

namespace Microsoft.AI.Foundry.Local.OpenAI.Responses;

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

using OfficialResponses = global::OpenAI.Responses;
using System.ClientModel.Primitives;

/// <summary>
/// Result returned by Foundry Local's list-responses extension endpoint.
/// </summary>
public sealed class ListResponsesResult
{
    public string ObjectType { get; init; } = "list";

    public IReadOnlyList<OfficialResponses.ResponseResult> Data { get; init; } = [];

    public string? FirstId { get; init; }

    public string? LastId { get; init; }

    public bool HasMore { get; init; }

    internal static ListResponsesResult FromJson(string json)
    {
        using var document = JsonDocument.Parse(json);
        var root = document.RootElement;

        var data = new List<OfficialResponses.ResponseResult>();
        if (root.TryGetProperty("data", out var dataElement) && dataElement.ValueKind == JsonValueKind.Array)
        {
            for (var i = 0; i < dataElement.GetArrayLength(); i++)
            {
                var item = dataElement[i];
                var parsed = ModelReaderWriter.Read<OfficialResponses.ResponseResult>(
                    new BinaryData(item.GetRawText()),
                    ModelReaderWriterOptions.Json,
                    global::OpenAI.OpenAIContext.Default);
                if (parsed is not null)
                {
                    data.Add(parsed);
                }
            }
        }

        return new ListResponsesResult
        {
            ObjectType = root.TryGetProperty("object", out var objectElement) ? objectElement.GetString() ?? "list" : "list",
            Data = data,
            FirstId = root.TryGetProperty("first_id", out var firstIdElement) ? firstIdElement.GetString() : null,
            LastId = root.TryGetProperty("last_id", out var lastIdElement) ? lastIdElement.GetString() : null,
            HasMore = root.TryGetProperty("has_more", out var hasMoreElement) && hasMoreElement.GetBoolean(),
        };
    }
}

/// <summary>
/// Convenience helpers that produce official <see cref="OfficialResponses.ResponseContentPart"/> instances for Foundry Local samples.
/// </summary>
public static class ResponseContentPartHelpers
{
    public static OfficialResponses.ResponseContentPart CreateInputImagePartFromFile(string path, string? detail = null)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            throw new ArgumentException("path must be non-empty.", nameof(path));
        }

        if (!File.Exists(path))
        {
            throw new FileNotFoundException($"Image file not found: {path}", path);
        }

        var mediaType = DetectImageMediaType(path);
        if (string.IsNullOrWhiteSpace(mediaType))
        {
            throw new ArgumentException($"Unable to infer image media type from file extension: {path}", nameof(path));
        }

        return CreateInputImagePartFromBytes(File.ReadAllBytes(path), mediaType, detail);
    }

    public static OfficialResponses.ResponseContentPart CreateInputImagePartFromBytes(byte[] data, string mediaType, string? detail = null)
    {
        if (data == null || data.Length == 0)
        {
            throw new ArgumentException("data must be non-empty.", nameof(data));
        }

        if (string.IsNullOrWhiteSpace(mediaType))
        {
            throw new ArgumentException("mediaType must be non-empty when using the official OpenAI binary image helper.", nameof(mediaType));
        }

        return OfficialResponses.ResponseContentPart.CreateInputImagePart(BinaryData.FromBytes(data, mediaType), ToImageDetailLevel(detail));
    }

    public static OfficialResponses.ResponseContentPart CreateInputImagePartFromUrl(string url, string? detail = null)
    {
        if (string.IsNullOrWhiteSpace(url))
        {
            throw new ArgumentException("url must be non-empty.", nameof(url));
        }

        return OfficialResponses.ResponseContentPart.CreateInputImagePart(new Uri(url), ToImageDetailLevel(detail));
    }

    public static string? DetectImageMediaType(string path)
    {
        var ext = Path.GetExtension(path).ToLowerInvariant();
        return ext switch
        {
            ".png" => "image/png",
            ".jpg" or ".jpeg" => "image/jpeg",
            ".gif" => "image/gif",
            ".webp" => "image/webp",
            ".bmp" => "image/bmp",
            _ => null,
        };
    }

    private static OfficialResponses.ResponseImageDetailLevel? ToImageDetailLevel(string? detail)
    {
        if (string.IsNullOrWhiteSpace(detail))
        {
            return null;
        }
        return new OfficialResponses.ResponseImageDetailLevel(detail);
    }
}

#pragma warning restore OPENAI001
