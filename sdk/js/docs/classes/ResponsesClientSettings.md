[foundry-local-sdk](../README.md) / ResponsesClientSettings

# Class: ResponsesClientSettings

Configuration settings for the Responses API client.
Properties use camelCase in JS and are serialized to snake_case for the API.

## Constructors

### Constructor

```ts
new ResponsesClientSettings(): ResponsesClientSettings;
```

#### Returns

`ResponsesClientSettings`

## Properties

### frequencyPenalty?

```ts
optional frequencyPenalty?: number;
```

***

### instructions?

```ts
optional instructions?: string;
```

System-level instructions to guide the model.

***

### maxOutputTokens?

```ts
optional maxOutputTokens?: number;
```

***

### metadata?

```ts
optional metadata?: Record<string, string>;
```

***

### parallelToolCalls?

```ts
optional parallelToolCalls?: boolean;
```

***

### presencePenalty?

```ts
optional presencePenalty?: number;
```

***

### reasoning?

```ts
optional reasoning?: ReasoningConfig;
```

***

### seed?

```ts
optional seed?: number;
```

***

### store?

```ts
optional store?: boolean;
```

***

### temperature?

```ts
optional temperature?: number;
```

***

### text?

```ts
optional text?: TextConfig;
```

***

### toolChoice?

```ts
optional toolChoice?: ResponseToolChoice;
```

***

### topP?

```ts
optional topP?: number;
```

***

### truncation?

```ts
optional truncation?: TruncationStrategy;
```
