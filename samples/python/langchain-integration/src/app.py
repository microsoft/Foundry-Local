# <complete_code>
# <imports>
import asyncio
from foundry_local_sdk import Configuration, FoundryLocalManager
from langchain_openai import ChatOpenAI
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.output_parsers import StrOutputParser
# </imports>


async def main():
    # <init>
    # Initialize the Foundry Local SDK
    config = Configuration(app_name="foundry_local_samples")
    await FoundryLocalManager.initialize(config)
    manager = FoundryLocalManager.instance

    # Download and register all execution providers.
    current_ep = ""
    def _ep_progress(ep_name: str, percent: float):
        nonlocal current_ep
        if ep_name != current_ep:
            if current_ep:
                print()
            current_ep = ep_name
        print(f"\r  {ep_name:<30}  {percent:5.1f}%", end="", flush=True)

    await manager.download_and_register_eps(progress_callback=_ep_progress)
    if current_ep:
        print()

    # Load a model
    model = await manager.catalog.get_model("qwen2.5-0.5b")
    await model.download(
        lambda progress: print(
            f"\rDownloading model: {progress:.2f}%",
            end="",
            flush=True,
        )
    )
    print()
    await model.load()
    print("Model loaded.")

    # Start the web service to expose an OpenAI-compatible endpoint
    await manager.start_web_service()
    base_url = f"{manager.urls[0]}/v1"
    # </init>

    # <langchain_setup>
    # Create a LangChain ChatOpenAI instance pointing to the local endpoint
    llm = ChatOpenAI(
        base_url=base_url,
        api_key="none",
        model=model.id,
    )
    # </langchain_setup>

    # <chat_completion>
    # Create a translation chain
    prompt = ChatPromptTemplate.from_messages([
        ("system", "You are a translator. Translate the following text to {language}. Only output the translation, nothing else."),
        ("user", "{text}")
    ])

    chain = prompt | llm | StrOutputParser()

    # Run the chain
    result = chain.invoke({"language": "Spanish", "text": "Hello, how are you today?"})
    print(f"Translation: {result}")
    # </chat_completion>

    # Clean up
    await model.unload()
    await manager.stop_web_service()


if __name__ == "__main__":
    asyncio.run(main())
# </complete_code>
