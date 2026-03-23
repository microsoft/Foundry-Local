# src/app/demos — Demo Modules
# Each demo showcases different MAF + Foundry Local capabilities

from .weather_tools import WeatherDemo
from .math_agent import MathAgentDemo
from .sentiment_analyzer import SentimentDemo
from .code_reviewer import CodeReviewerDemo
from .multi_agent_debate import DebateDemo
from .registry import DEMO_REGISTRY, get_demo, list_demos

__all__ = [
    "WeatherDemo",
    "MathAgentDemo",
    "SentimentDemo",
    "CodeReviewerDemo",
    "DebateDemo",
    "DEMO_REGISTRY",
    "get_demo",
    "list_demos",
]
