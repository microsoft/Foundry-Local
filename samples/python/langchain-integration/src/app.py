# <complete_code>
# <imports>
from foundry_local import FoundryLocalManager
from langchain_openai import ChatOpenAI
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.output_parsers import StrOutputParser
# </imports>

# <init>
alias = "qwen2.5-0.5b"
manager = FoundryLocalManager(alias)
# </init>

# <langchain_setup>
# Create a LangChain ChatOpenAI instance pointing to the local endpoint
llm = ChatOpenAI(
    base_url=manager.endpoint,
    api_key=manager.api_key,
    model=manager.get_model_info(alias).id,
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
# </complete_code>
