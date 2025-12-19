using Microsoft.AI.Foundry.Local;
using Microsoft.AI.Foundry.Local.Detail;
using System.ClientModel;
using System.ClientModel.Primitives;
using System.Reflection;

/// <summary>
/// A custom <see cref="PipelineTransport"/> that conducts pipeline traffic via Foundry Local CoreInterop instead of HTTP
/// network traffic.
/// </summary>
/// <remarks>
/// As written, this relies on Reflection for access to non-public CoreInterop fields and methods. In an integrated form,
/// CoreInterop could instead be used directly via the <see cref="Model"/>'s CoreInterop instance (or another CoreInterop
/// instance).
/// </remarks>
internal class FoundryLocalPipelineTransport : PipelineTransport
{
    private readonly FoundryLocalInteropWrapper _interopWrapper;

    public FoundryLocalPipelineTransport(Model foundryLocalModel)
    {
        _interopWrapper = new(foundryLocalModel);
    }

    protected override PipelineMessage CreateMessageCore()
    {
        return new FoundryLocalPipelineMessage(new FoundryLocalPipelineRequest());
    }

    protected override void ProcessCore(PipelineMessage message)
    {
        if (message is FoundryLocalPipelineMessage foundryLocalMessage)
        {
            BinaryData interopResultBytes = _interopWrapper.ExecuteInteropChat(message);
            foundryLocalMessage.SetResponse(new FoundryLocalPipelineResponse(interopResultBytes));
        }
        else
        {
            throw new NotImplementedException();
        }
    }

    protected override async ValueTask ProcessCoreAsync(PipelineMessage message)
    {
        if (message is FoundryLocalPipelineMessage foundryLocalMessage)
        {
            BinaryData interopResultBytes = await _interopWrapper.ExecuteInteropChatAsync(message);
            foundryLocalMessage.SetResponse(new FoundryLocalPipelineResponse(interopResultBytes));
        }
        else
        {
            throw new NotImplementedException();
        }
    }
    
    private class FoundryLocalPipelineRequestHeaders : PipelineRequestHeaders
    {
        public override void Add(string name, string value) => throw new NotImplementedException();
        public override IEnumerator<KeyValuePair<string, string>> GetEnumerator() => throw new NotImplementedException();
        public override bool Remove(string name) => throw new NotImplementedException();

        public override void Set(string name, string value)
        {
        }

        public override bool TryGetValue(string name, out string? value)
        {
            value = null;
            return false;
        }

        public override bool TryGetValues(string name, out IEnumerable<string>? values) => throw new NotImplementedException();
    }

    private class FoundryLocalPipelineRequest : PipelineRequest
    {
        protected override string MethodCore { get; set; } = "POST";
        protected override Uri? UriCore { get; set; }
        protected override PipelineRequestHeaders HeadersCore { get; } = new FoundryLocalPipelineRequestHeaders();
        protected override BinaryContent? ContentCore { get; set; }

        public override void Dispose()
        {
        }
    }

    private class FoundryLocalPipelineResponse : PipelineResponse
    {
        public FoundryLocalPipelineResponse(BinaryData interopResultBytes)
        {
            ContentStream = interopResultBytes.ToStream();
        }

        public override int Status => 200;
        public override string ReasonPhrase => throw new NotImplementedException();
        public override Stream? ContentStream { get; set; }

        public override BinaryData Content
        {
            get
            {
                if (_content is null && ContentStream is not null)
                {
                    _content = BinaryData.FromStream(ContentStream);
                    ContentStream.Position = 0;
                }
                return _content ??= BinaryData.Empty;
            }
        }
        private BinaryData? _content;

        protected override PipelineResponseHeaders HeadersCore => throw new NotImplementedException();
        public override BinaryData BufferContent(CancellationToken cancellationToken = default) => Content;
        public override ValueTask<BinaryData> BufferContentAsync(CancellationToken cancellationToken = default) => ValueTask.FromResult(Content);
        public override void Dispose()
        {
        }
    }

    private class FoundryLocalPipelineMessage : PipelineMessage
    {
        public FoundryLocalPipelineMessage(PipelineRequest request)
            : base(request)
        {
        }

        public void SetResponse(FoundryLocalPipelineResponse response)
        {
            Response = response;
        }
    }

    private class FoundryLocalInteropWrapper
    {
        private readonly object _coreInteropField;
        private readonly MethodInfo _executeCommandInfo;
        private readonly MethodInfo _executeCommandAsyncInfo;
        
        public FoundryLocalInteropWrapper(Model foundryLocalModel)
        {
            _coreInteropField = typeof(ModelVariant)
                .GetField("_coreInterop", BindingFlags.Instance | BindingFlags.NonPublic)
                ?.GetValue(foundryLocalModel.SelectedVariant)
                ?? throw new InvalidOperationException();
            _executeCommandInfo = _coreInteropField
                .GetType()
                ?.GetMethod("ExecuteCommand", BindingFlags.Instance | BindingFlags.Public, [typeof(string), typeof(CoreInteropRequest)])
                    ?? throw new InvalidOperationException();
            _executeCommandAsyncInfo = _coreInteropField
                .GetType()
                ?.GetMethod("ExecuteCommandAsync", BindingFlags.Instance | BindingFlags.Public, [typeof(string), typeof(CoreInteropRequest), typeof(CancellationToken?)])
                    ?? throw new InvalidOperationException();
        }

        public BinaryData ExecuteInteropChat(PipelineMessage? message)
        {
            if (message is FoundryLocalPipelineMessage foundryLocalMessage
                && message?.Request?.Content is BinaryContent requestContent)
            {
                CoreInteropRequest interopRequest = CreateInteropRequest(requestContent);
                object reflectedInteropResult = _executeCommandInfo.Invoke(_coreInteropField, ["chat_completions", interopRequest]) ?? throw new InvalidOperationException();
                return GetInteropResultBytes(reflectedInteropResult);
            }
            else
            {
                throw new NotImplementedException();
            }
        }

        public async Task<BinaryData> ExecuteInteropChatAsync(PipelineMessage? message)
        {
            if (message is FoundryLocalPipelineMessage foundryLocalMessage
                && message?.Request?.Content is BinaryContent requestContent)
            {
                CoreInteropRequest interopRequest = CreateInteropRequest(requestContent);
                dynamic interopResultTask = _executeCommandAsyncInfo.Invoke(_coreInteropField, ["chat_completions", interopRequest, message.CancellationToken]) ?? throw new InvalidOperationException();
                await interopResultTask;
                object reflectedInteropResult = interopResultTask.GetType().GetProperty("Result").GetValue(interopResultTask);
                return GetInteropResultBytes(reflectedInteropResult);
            }
            else
            {
                throw new NotImplementedException();
            }
        }

        private static CoreInteropRequest CreateInteropRequest(BinaryContent content)
        {
            using MemoryStream contentStream = new();
            content.WriteTo(contentStream);
            contentStream.Flush();
            contentStream.Position = 0;
            using StreamReader contentReader = new(contentStream);
            string rawContent = contentReader.ReadToEnd();

            return new CoreInteropRequest()
            {
                Params = new()
                {
                    ["OpenAICreateRequest"] = rawContent
                }
            };
        }
        
        private static BinaryData GetInteropResultBytes(object reflectedInteropResult)
        {
            object? reflectedData = reflectedInteropResult
                ?.GetType()
                ?.GetField("Data", BindingFlags.Instance | BindingFlags.NonPublic)
                ?.GetValue(reflectedInteropResult);
            if (reflectedData is string rawReflectedData)
            {
                return BinaryData.FromString(rawReflectedData);
            }
            return BinaryData.Empty;
        }
    }
}