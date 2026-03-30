# <complete_code>
# <imports>
from foundry_local_sdk import Configuration, FoundryLocalManager
from langchain_openai import ChatOpenAI
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.output_parsers import StrOutputParser
# </imports>

# <init>
# Initialize the Foundry Local SDK
config = Configuration(app_name="foundry_local_samples")
FoundryLocalManager.initialize(config)
manager = FoundryLocalManager.instance

# Load a model
model = manager.catalog.get_model("qwen2.5-0.5b")
model.download(
    lambda progress: print(
        f"\rDownloading model: {progress:.2f}%",
        end="",
        flush=True,
    )
)
print()
model.load()
print("Model loaded.")

# Start the web service to expose an OpenAI-compatible endpoint
manager.start_web_service()
base_url = manager.urls[0]
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
model.unload()
manager.stop_web_service()
# </complete_code>
