"""
Demo: Sentiment Analyzer
────────────────────────
Demonstrates text analysis tools for sentiment and emotion detection.
"""

from __future__ import annotations

import re
from collections import Counter
from typing import Annotated

from agent_framework import ChatAgent
from agent_framework.openai import OpenAIChatClient
from pydantic import Field

from ..foundry_boot import FoundryConnection
from .registry import DemoInfo, register_demo

# ─── Lexicon ─────────────────────────────────────────────────────

POSITIVE_WORDS = {
    "good", "great", "excellent", "amazing", "wonderful", "fantastic",
    "happy", "love", "best", "awesome", "brilliant", "perfect",
    "beautiful", "outstanding", "superb", "delightful", "pleased",
    "satisfied", "excited", "thankful", "grateful", "impressed",
}

NEGATIVE_WORDS = {
    "bad", "terrible", "awful", "horrible", "poor", "worst",
    "sad", "hate", "disappointed", "angry", "frustrated", "annoyed",
    "ugly", "boring", "waste", "useless", "broken", "failed",
    "difficult", "confusing", "slow", "expensive", "problem",
}

EMOTION_PATTERNS = {
    "joy": ["happy", "excited", "delighted", "thrilled", "pleased", "love", "wonderful"],
    "sadness": ["sad", "disappointed", "unhappy", "depressed", "lonely", "miss", "sorry"],
    "anger": ["angry", "frustrated", "annoyed", "furious", "mad", "hate", "outraged"],
    "fear": ["afraid", "scared", "worried", "anxious", "nervous", "terrified", "panic"],
    "surprise": ["surprised", "amazed", "astonished", "shocked", "unexpected", "wow"],
    "trust": ["trust", "believe", "reliable", "confident", "safe", "secure", "honest"],
}


# ─── Tool Functions ──────────────────────────────────────────────

def analyze_sentiment(
    text: Annotated[str, Field(description="The text to analyze for sentiment")],
) -> str:
    """Analyze the overall sentiment of text."""
    words = re.findall(r"\b\w+\b", text.lower())
    pos_count = sum(1 for w in words if w in POSITIVE_WORDS)
    neg_count = sum(1 for w in words if w in NEGATIVE_WORDS)
    total = pos_count + neg_count
    if total == 0:
        sentiment, confidence, score = "neutral", 0.5, 0.0
    else:
        score = (pos_count - neg_count) / total
        if score > 0.2:
            sentiment = "positive"
        elif score < -0.2:
            sentiment = "negative"
        else:
            sentiment = "neutral"
        confidence = min(0.95, 0.5 + abs(score) * 0.5)
    return (
        f"Sentiment Analysis:\n"
        f"  Overall: {sentiment.upper()}\n"
        f"  Score: {score:+.2f} (range: -1.0 to +1.0)\n"
        f"  Confidence: {confidence:.0%}\n"
        f"  Positive words found: {pos_count}\n"
        f"  Negative words found: {neg_count}"
    )


def detect_emotions(
    text: Annotated[str, Field(description="The text to analyze for emotions")],
) -> str:
    """Detect specific emotions present in the text."""
    words = set(re.findall(r"\b\w+\b", text.lower()))
    detected = []
    for emotion, keywords in EMOTION_PATTERNS.items():
        matches = words.intersection(keywords)
        if matches:
            detected.append((emotion, len(matches), list(matches)))
    if not detected:
        return "No strong emotions detected in the text."
    detected.sort(key=lambda x: x[1], reverse=True)
    lines = ["Emotions detected:"]
    for emotion, count, matches in detected:
        intensity = "strong" if count >= 2 else "mild"
        lines.append(f"  \u2022 {emotion.title()} ({intensity}): triggered by '{', '.join(matches)}'")
    return "\n".join(lines)


def extract_key_phrases(
    text: Annotated[str, Field(description="The text to extract key phrases from")],
) -> str:
    """Extract and rate important phrases from text."""
    sentences = re.split(r"[.!?]+", text)
    results = []
    for sent in sentences:
        sent = sent.strip()
        if len(sent) < 10:
            continue
        words = re.findall(r"\b\w+\b", sent.lower())
        pos = sum(1 for w in words if w in POSITIVE_WORDS)
        neg = sum(1 for w in words if w in NEGATIVE_WORDS)
        if pos > neg:
            rating = "positive"
        elif neg > pos:
            rating = "negative"
        else:
            rating = "neutral"
        display = sent[:80] + "\u2026" if len(sent) > 80 else sent
        results.append(f'  [{rating:^8}] "{display}"')
    if not results:
        return "No significant phrases found."
    return "Key phrases:\n" + "\n".join(results[:5])


def compare_sentiment(
    text1: Annotated[str, Field(description="First text to compare")],
    text2: Annotated[str, Field(description="Second text to compare")],
) -> str:
    """Compare sentiment between two texts."""
    def score_text(text):
        words = re.findall(r"\b\w+\b", text.lower())
        pos = sum(1 for w in words if w in POSITIVE_WORDS)
        neg = sum(1 for w in words if w in NEGATIVE_WORDS)
        total = pos + neg
        return (pos - neg) / total if total > 0 else 0

    s1 = score_text(text1)
    s2 = score_text(text2)

    def label(s):
        if s > 0.2:
            return "positive"
        if s < -0.2:
            return "negative"
        return "neutral"

    diff = abs(s1 - s2)
    if diff < 0.1:
        comparison = "Both texts have similar sentiment"
    elif s1 > s2:
        comparison = f"Text 1 is more positive (by {diff:.2f})"
    else:
        comparison = f"Text 2 is more positive (by {diff:.2f})"
    return (
        f"Sentiment Comparison:\n"
        f"  Text 1: {label(s1)} ({s1:+.2f})\n"
        f"  Text 2: {label(s2)} ({s2:+.2f})\n"
        f"  \u2192 {comparison}"
    )


def word_frequency(
    text: Annotated[str, Field(description="The text to analyze")],
    top_n: Annotated[int, Field(description="Number of top words to return")] = 10,
) -> str:
    """Get the most frequent meaningful words in text."""
    stopwords = {
        "the", "a", "an", "is", "are", "was", "were", "be", "been", "being",
        "have", "has", "had", "do", "does", "did", "will", "would", "could",
        "should", "may", "might", "must", "shall", "can", "need", "dare",
        "to", "of", "in", "for", "on", "with", "at", "by", "from", "up",
        "about", "into", "over", "after", "it", "its", "this", "that",
        "and", "but", "or", "nor", "so", "yet", "both", "either", "neither",
        "i", "me", "my", "myself", "we", "our", "you", "your", "he", "she",
    }
    words = re.findall(r"\b\w+\b", text.lower())
    words = [w for w in words if w not in stopwords and len(w) > 2]
    counter = Counter(words)
    top = counter.most_common(top_n)
    if not top:
        return "No significant words found."
    lines = [f"Top {min(top_n, len(top))} words:"]
    for word, count in top:
        bar = "\u2588" * min(count, 20)
        lines.append(f"  {word:15} {count:3} {bar}")
    return "\n".join(lines)


# ─── Demo Class ──────────────────────────────────────────────────

class SentimentDemo:
    def __init__(self, conn: FoundryConnection):
        self.conn = conn
        self.agent = self._create_agent()

    def _create_agent(self) -> ChatAgent:
        client = OpenAIChatClient(
            api_key=self.conn.api_key,
            base_url=self.conn.endpoint,
            model_id=self.conn.model_id,
        )
        return ChatAgent(
            chat_client=client,
            name="SentimentAnalyst",
            instructions=(
                "You are a text analysis expert. Use the provided tools to analyze text:\n\n"
                "  \u2022 analyze_sentiment: Get overall sentiment score\n"
                "  \u2022 detect_emotions: Find specific emotions\n"
                "  \u2022 extract_key_phrases: Find important phrases\n"
                "  \u2022 compare_sentiment: Compare two texts\n"
                "  \u2022 word_frequency: Find common words\n\n"
                "When asked to analyze text, use the appropriate tool(s). "
                "Summarize findings in plain language after using tools."
            ),
            tools=[analyze_sentiment, detect_emotions, extract_key_phrases, compare_sentiment, word_frequency],
        )

    async def run(self, prompt: str) -> dict:
        import time
        t0 = time.perf_counter()
        result = await self.agent.run(prompt)
        elapsed = time.perf_counter() - t0
        text = re.sub(r"<tool_call>.*?</tool_call>\s*", "", str(result), flags=re.DOTALL).strip()
        return {
            "prompt": prompt,
            "response": text,
            "elapsed": round(elapsed, 2),
            "tools_available": ["analyze_sentiment", "detect_emotions", "extract_key_phrases", "compare_sentiment", "word_frequency"],
        }


# ─── Register ────────────────────────────────────────────────────

async def run_sentiment_demo(conn: FoundryConnection, prompt: str) -> dict:
    demo = SentimentDemo(conn)
    return await demo.run(prompt)


register_demo(DemoInfo(
    id="sentiment_analyzer",
    name="Sentiment Analyzer",
    description="Text analysis agent that detects sentiment, emotions, key phrases, and word frequency.",
    icon="💬",
    category="Tool Calling",
    runner=run_sentiment_demo,
    tags=["tools", "function-calling", "text-analysis", "single-agent"],
    suggested_prompt="Analyze this review: 'The product arrived quickly and the quality exceeded my expectations. However, the packaging was disappointing and customer support was slow to respond. Overall I'm satisfied but not thrilled.'",
))
