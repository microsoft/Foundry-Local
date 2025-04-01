## Using the OpenAI Node.js SDK

```javascript
// Install with: npm install openai
import OpenAI from 'openai';

const openai = new OpenAI({
  baseURL: 'http://localhost:5272/v1',
  apiKey: 'not-needed-for-local'
});

async function generateText() {
  const response = await openai.chat.completions.create({
    model: 'Phi-4-mini-gpu-int4-rtn-block-32',
    messages: [
      { role: 'user', content: 'How can I integrate AI Foundry Local with my app?' }
    ],
  });

  console.log(response.choices[0].message.content);
}

generateText();
```

## Using Fetch API

```javascript
async function queryModel() {
  const response = await fetch('http://localhost:5272/v1/chat/completions', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      model: 'Phi-4-mini-gpu-int4-rtn-block-32',
      messages: [
        { role: 'user', content: 'What are the advantages of AI Foundry Local?' }
      ]
    }),
  });
  
  const data = await response.json();
  console.log(data.choices[0].message.content);
}

queryModel();
```

## Streaming Responses

### Using OpenAI SDK

```javascript
// Install with: npm install openai
import OpenAI from 'openai';

const openai = new OpenAI({
  baseURL: 'http://localhost:5272/v1',
  apiKey: 'not-needed-for-local'
});

async function streamCompletion() {
  const stream = await openai.chat.completions.create({
    model: 'Phi-4-mini-gpu-int4-rtn-block-32',
    messages: [{ role: 'user', content: 'Write a short story about AI' }],
    stream: true,
  });
  
  for await (const chunk of stream) {
    if (chunk.choices[0]?.delta?.content) {
      process.stdout.write(chunk.choices[0].delta.content);
    }
  }
}

streamCompletion();
```

### Using Fetch API and ReadableStream

```javascript
async function streamWithFetch() {
  const response = await fetch('http://localhost:5272/v1/chat/completions', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Accept': 'text/event-stream',
    },
    body: JSON.stringify({
      model: 'Phi-4-mini-gpu-int4-rtn-block-32',
      messages: [{ role: 'user', content: 'Write a short story about AI' }],
      stream: true,
    }),
  });

  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    
    const chunk = decoder.decode(value);
    const lines = chunk.split('\n').filter(line => line.trim() !== '');
    
    for (const line of lines) {
      if (line.startsWith('data: ')) {
        const data = line.substring(6);
        if (data === '[DONE]') continue;
        
        try {
          const json = JSON.parse(data);
          const content = json.choices[0]?.delta?.content || '';
          if (content) {
            // Print to console without line breaks, similar to process.stdout.write
            process.stdout.write(content);
          }
        } catch (e) {
          console.error('Error parsing JSON:', e);
        }
      }
    }
  }
}

// Call the function to start streaming
streamWithFetch();
```
