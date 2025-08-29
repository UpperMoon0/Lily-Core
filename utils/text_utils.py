#!/usr/bin/env python3
"""
Text Utilities for Lily-Core

Utility functions for text processing, formatting, and validation.
"""

import re
from typing import List, Optional


def clean_text(text: str) -> str:
    """
    Clean and normalize text input.

    Args:
        text: Input text to clean

    Returns:
        str: Cleaned text
    """
    if not text:
        return ""

    # Remove excessive whitespace
    text = re.sub(r'\s+', ' ', text.strip())

    # Remove control characters
    text = re.sub(r'[\x00-\x1f\x7f-\x9f]', '', text)

    return text


def truncate_text(text: str, max_length: int = 100, suffix: str = "...") -> str:
    """
    Truncate text to a maximum length with optional suffix.

    Args:
        text: Text to truncate
        max_length: Maximum length of the result
        suffix: Suffix to add if text is truncated

    Returns:
        str: Truncated text
    """
    if not text or len(text) <= max_length:
        return text

    if len(suffix) >= max_length:
        return suffix[:max_length]

    return text[:max_length - len(suffix)] + suffix


def split_into_sentences(text: str) -> List[str]:
    """
    Split text into sentences using basic sentence boundary detection.

    Args:
        text: Text to split

    Returns:
        List[str]: List of sentences
    """
    # Basic sentence splitting (can be improved with NLP libraries)
    sentences = re.split(r'(?<=[.!?])\s+', text.strip())

    # Filter out empty sentences
    return [s for s in sentences if s.strip()]


def contains_keywords(text: str, keywords: List[str], case_sensitive: bool = False) -> bool:
    """
    Check if text contains any of the given keywords.

    Args:
        text: Text to check
        keywords: List of keywords to search for
        case_sensitive: Whether the search should be case sensitive

    Returns:
        bool: True if any keyword is found
    """
    if case_sensitive:
        return any(keyword in text for keyword in keywords)
    else:
        text_lower = text.lower()
        return any(keyword.lower() in text_lower for keyword in keywords)


def extract_urls(text: str) -> List[str]:
    """
    Extract URLs from text using regex.

    Args:
        text: Text to extract URLs from

    Returns:
        List[str]: List of URLs found in the text
    """
    url_pattern = re.compile(
        r'http[s]?://'  # http:// or https://
        r'(?:[a-zA-Z]|[0-9]|[$-_@.&+]|[!*\\(\\),]|'  # domain characters
        r'(?:%[0-9a-fA-F][0-9a-fA-F]))+'  # URL encoded characters
    )

    return url_pattern.findall(text)


def is_question(text: str) -> bool:
    """
    Determine if text appears to be a question.

    Args:
        text: Text to analyze

    Returns:
        bool: True if text appears to be a question
    """
    text = text.strip()

    # Check for question marks
    if text.endswith('?'):
        return True

    # Check for question words
    question_words = [
        'what', 'when', 'where', 'who', 'whom', 'whose',
        'which', 'why', 'how', 'how many', 'how much',
        'can you', 'could you', 'would you', 'will you',
        'do you', 'does', 'did', 'are you', 'is it',
        'am i', 'have you', 'has', 'had'
    ]

    text_lower = text.lower()
    return any(text_lower.startswith(qw) for qw in question_words)


def format_conversation_context(messages: List[dict], max_chars_per_message: int = 200) -> str:
    """
    Format conversation messages into a context string.

    Args:
        messages: List of message dictionaries
        max_chars_per_message: Maximum characters per message in context

    Returns:
        str: Formatted conversation context
    """
    context_lines = []

    for msg in messages:
        role = msg.get('role', 'unknown').capitalize()
        content = msg.get('content', '')

        # Truncate content if too long
        if len(content) > max_chars_per_message:
            content = content[:max_chars_per_message] + "..."

        context_lines.append(f"{role}: {content}")

    return "\n".join(context_lines)


def sanitize_string(input_str: str, max_length: Optional[int] = None) -> str:
    """
    Sanitize a string by removing potentially harmful characters.

    Args:
        input_str: String to sanitize
        max_length: Maximum allowed length

    Returns:
        str: Sanitized string
    """
    if not input_str:
        return ""

    # Remove potentially dangerous characters
    sanitized = re.sub(r'[<>"\'{}|\\\\]', '', input_str)

    # Limit length if specified
    if max_length and len(sanitized) > max_length:
        sanitized = sanitized[:max_length]

    return sanitized.strip()