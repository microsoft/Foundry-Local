using Microsoft.AI.Foundry.Local;
using OpenAI.Chat;
using System.ClientModel;

/// <summary>
/// A simple, derived <see cref="ChatClient"/> that applies a custom <see cref="System.ClientModel.Primitives.PipelineTransport"/>
/// to redirect traffic via Foundry Local Model CoreInterop.
/// </summary>
/// <remarks>
/// For externally-applied demonstration, this derived client is made public. In an integrated form, Foundry Local could instead
/// return the parent <see cref="ChatClient"/> type from the OpenAI library and use this type internally for the concrete instance.
/// </remarks>
public class FoundryLocalChatClient : ChatClient
{
    public FoundryLocalChatClient(Model foundryLocalModel)
        : base(foundryLocalModel.Id, new ApiKeyCredential("placeholder"), CreateClientOptions(foundryLocalModel))
    {
    }

    private static OpenAI.OpenAIClientOptions CreateClientOptions(Model foundryLocalModel)
    {
        return new()
        {
            Transport = new FoundryLocalPipelineTransport(foundryLocalModel)
        };
    }
}