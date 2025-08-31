#!/usr/bin/env python3
"""
Agent Loop Service for Lily-Core

Implements a sophisticated agent loop architecture (Reason-Act-Observe) for
multi-step task planning, tool orchestration, and iterative problem solving.
"""

import time
import asyncio
from typing import Dict, List, Optional, Any, Tuple
from datetime import datetime
from dataclasses import dataclass, field
from enum import Enum

from core.config import model, chat_settings


class AgentState(Enum):
    """States in the agent loop lifecycle."""
    INITIALIZING = "initializing"
    REASONING = "reasoning"
    PLANNING = "planning"
    ACTING = "acting"
    OBSERVING = "observing"
    COMPLETING = "completing"
    TERMINATED = "terminated"


class LoopAction(Enum):
    """Actions that can be taken in each loop iteration."""
    EXECUTE_TOOL = "execute_tool"
    GENERATE_RESPONSE = "generate_response"
    ASK_CLARIFICATION = "ask_clarification"
    UPDATE_CONTEXT = "update_context"
    TERMINATE = "terminate"


@dataclass
class LoopContext:
    """Context maintained across agent loop iterations."""
    user_id: str
    user_message: str
    conversation_history: List[Dict[str, Any]] = field(default_factory=list)
    current_step: int = 0
    max_steps: int = 5
    working_memory: Dict[str, Any] = field(default_factory=dict)
    tool_results: List[Dict[str, Any]] = field(default_factory=list)
    plan: List[Dict[str, Any]] = field(default_factory=list)
    final_response: Optional[str] = None
    state: AgentState = AgentState.INITIALIZING
    confidence: float = 0.0
    reasoning: str = ""


@dataclass
class AgentStep:
    """Represents a single step in the agent loop."""
    step_number: int
    action: LoopAction
    reasoning: str
    parameters: Dict[str, Any] = field(default_factory=dict)
    tool_name: Optional[str] = None
    tool_params: Dict[str, Any] = field(default_factory=dict)
    result: Optional[Any] = None
    success: bool = False
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())


class AgentLoopOrchestrator:
    """
    Main orchestrator for the agent loop architecture.
    Implements the Reason-Act-Observe pattern with multi-step planning.
    """

    def __init__(self):
        """Initialize the agent loop orchestrator."""
        self.memory_service = None
        self.tool_service = None
        self.current_context: Optional[LoopContext] = None

    def set_services(self, memory_service, tool_service):
        """Inject required service dependencies."""
        self.memory_service = memory_service
        self.tool_service = tool_service

    async def execute_agent_loop(self, user_message: str, user_id: str = "default_user") -> Dict[str, Any]:
        """
        Execute the complete agent loop for a user query.

        Args:
            user_message: The user's message
            user_id: User identifier

        Returns:
            dict: Final response with loop execution details
        """
        # Initialize loop context
        self.current_context = LoopContext(
            user_id=user_id,
            user_message=user_message,
            conversation_history=self._get_conversation_history(user_id),
            max_steps=chat_settings.max_loop_steps if hasattr(chat_settings, 'max_loop_steps') else 5
        )

        print(f"🤖 Starting agent loop for: {user_message[:100]}...")

        try:
            # Main agent loop
            while not self._should_terminate():
                await self._execute_loop_step()

                if self.current_context.current_step >= self.current_context.max_steps:
                    break

            # Generate final response
            if self.current_context.final_response is None:
                self.current_context.final_response = await self._generate_final_response()

            return self._build_response()

        except Exception as e:
            print(f"❌ Agent loop failed: {e}")
            return self._build_error_response(str(e))

    async def _execute_loop_step(self):
        """Execute a single step in the agent loop."""
        self.current_context.current_step += 1

        # REASON: Analyze current state and plan next action
        step = await self._reasoning_phase()
        step.step_number = self.current_context.current_step

        # ACT: Execute the planned action
        if step.action == LoopAction.EXECUTE_TOOL:
            await self._action_phase(step)
        elif step.action == LoopAction.GENERATE_RESPONSE:
            await self._response_generation_phase(step)
        elif step.action == LoopAction.ASK_CLARIFICATION or step.action == LoopAction.TERMINATE:
            pass  # Handle in termination logic

        # OBSERVE: Process results and update context
        await self._observation_phase(step)

    async def _reasoning_phase(self) -> AgentStep:
        """
        REASONING PHASE: Analyze current context and decide next action.

        Uses LLM to plan the next step based on:
        - Current task progress
        - Available information
        - Previous tool results
        - User's original intent
        """
        self.current_context.state = AgentState.REASONING

        prompt = self._build_reasoning_prompt()

        try:
            response = model.generate_content(prompt)
            reasoning_data = self._parse_reasoning_response(response.text)

            step = AgentStep(
                step_number=self.current_context.current_step,
                action=reasoning_data['action'],
                reasoning=reasoning_data['reasoning'],
                tool_name=reasoning_data.get('tool_name'),
                tool_params=reasoning_data.get('tool_params', {})
            )

            self.current_context.confidence = reasoning_data.get('confidence', 0.5)
            print(f"🧠 Step {self.current_context.current_step}: {step.action.value} - {step.reasoning[:80]}...")

            return step

        except Exception as e:
            # Fallback to conservative action
            return AgentStep(
                step_number=self.current_context.current_step,
                action=LoopAction.GENERATE_RESPONSE,
                reasoning=f"Fallback due to reasoning error: {str(e)}"
            )

    def _get_conversation_history(self, user_id: str) -> List[Dict[str, Any]]:
        """Get relevant conversation history for context."""
        if self.memory_service:
            try:
                return self.memory_service.get_conversation_history(user_id, limit=10)
            except:
                pass
        return []

    def _build_reasoning_prompt(self) -> str:
        """Build the LLM prompt for the reasoning phase."""
        context = self.current_context
        plan_progress = self._summarize_progress()

        prompt = f"""You are Lily, an intelligent AI assistant using advanced reasoning to help users.

CURRENT TASK:
User Query: {context.user_message}

CONTEXT:
- Step: {context.current_step}/{context.max_steps}
- Progress: {plan_progress}
- Working Memory: {context.working_memory}
- Previous Tool Results: {len(context.tool_results)} results available

REASONING TASK:
1. Analyze the current progress toward answering: "{context.user_message}"
2. Determine what information or action is still needed
3. Decide the optimal next action

AVAILABLE ACTIONS:
- EXECUTE_TOOL: Use a specific tool (specify tool_name and parameters)
- GENERATE_RESPONSE: Provide the final answer to the user
- ASK_CLARIFICATION: Need more information from user
- TERMINATE: Task is complete, exit loop

RESPONSE FORMAT:
ACTION: [EXECUTE_TOOL|GENERATE_RESPONSE|ASK_CLARIFICATION|TERMINATE]
TOOL_NAME: [tool name if EXECUTE_TOOL]
TOOL_PARAMS: [JSON parameters if EXECUTE_TOOL]
CONFIDENCE: [0.0-1.0]
REASONING: [2-3 sentences explaining your decision]

Make your decision:"""

        return prompt

    def _summarize_progress(self) -> str:
        """Summarize current progress toward completing the task."""
        if not self.current_context.tool_results:
            return "No actions taken yet - need to start working on the query"

        results_count = len(self.current_context.tool_results)
        last_result = self.current_context.tool_results[-1] if self.current_context.tool_results else {}

        return f"Executed {results_count} tool actions. Latest result: {last_result.get('summary', 'No summary available')[:100]}..."

    def _parse_reasoning_response(self, response_text: str) -> Dict[str, Any]:
        """Parse the LLM reasoning response."""
        lines = response_text.split('\n')
        result = {
            'action': LoopAction.GENERATE_RESPONSE,
            'confidence': 0.5,
            'reasoning': response_text
        }

        for line in lines:
            if line.startswith('ACTION:'):
                action_str = line.replace('ACTION:', '').strip().upper()
                if 'EXECUTE_TOOL' in action_str:
                    result['action'] = LoopAction.EXECUTE_TOOL
                elif 'GENERATE_RESPONSE' in action_str:
                    result['action'] = LoopAction.GENERATE_RESPONSE
                elif 'ASK_CLARIFICATION' in action_str:
                    result['action'] = LoopAction.ASK_CLARIFICATION
                elif 'TERMINATE' in action_str:
                    result['action'] = LoopAction.TERMINATE
            elif line.startswith('TOOL_NAME:'):
                result['tool_name'] = line.replace('TOOL_NAME:', '').strip()
            elif line.startswith('TOOL_PARAMS:'):
                # Simple JSON parsing - in production use proper JSON parser
                params_str = line.replace('TOOL_PARAMS:', '').strip()
                try:
                    result['tool_params'] = eval(params_str)  # Be careful with eval in production
                except:
                    result['tool_params'] = {}
            elif line.startswith('CONFIDENCE:'):
                try:
                    result['confidence'] = float(line.replace('CONFIDENCE:', '').strip())
                except:
                    result['confidence'] = 0.5
            elif line.startswith('REASONING:'):
                result['reasoning'] = line.replace('REASONING:', '').strip()

        return result

    async def _action_phase(self, step: AgentStep):
        """ACTION PHASE: Execute the planned action."""
        self.current_context.state = AgentState.ACTING

        if step.tool_name and self.tool_service:
            try:
                print(f"🔧 Executing tool: {step.tool_name} with params: {step.tool_params}")

                # Execute the tool
                result = await self.tool_service.call_tool_by_name(
                    step.tool_name,
                    **step.tool_params
                )

                # Store result in context
                self.current_context.tool_results.append({
                    'step': self.current_context.current_step,
                    'tool_name': step.tool_name,
                    'params': step.tool_params,
                    'result': result,
                    'timestamp': datetime.now().isoformat()
                })

                step.result = result
                step.success = 'error' not in result

                print(f"✅ Tool execution {'successful' if step.success else 'failed'}")

            except Exception as e:
                step.result = {'error': str(e)}
                step.success = False
                print(f"❌ Tool execution error: {e}")

    async def _response_generation_phase(self, step: AgentStep):
        """Generate the final response based on all collected information."""
        self.current_context.state = AgentState.COMPLETING

        prompt = self._build_response_generation_prompt()

        try:
            response = model.generate_content(prompt)
            self.current_context.final_response = response.text
            step.success = True

        except Exception as e:
            self.current_context.final_response = f"I encountered an error while generating the response: {str(e)}"
            step.success = False

    def _build_response_generation_prompt(self) -> str:
        """Build prompt for final response generation."""
        context = self.current_context
        tool_info = ""

        if context.tool_results:
            tool_info = "\n".join([
                f"Tool {i+1} ({result['tool_name']}): {result['result'].get('summary', 'No summary')[:200]}..."
                for i, result in enumerate(context.tool_results)
            ])

        prompt = f"""You are Lily, a helpful AI assistant.

USER QUERY: {context.user_message}

TOOL RESULTS (if any):
{tool_info}

CONVERSATION CONTEXT:
{context.working_memory}

TASK: Provide a comprehensive, natural response that addresses the user's query using any relevant information from the tool results. If no tool results are available, respond conversationally based on your knowledge.

RESPONSE GUIDELINES:
- Be helpful and informative
- Acknowledge any tool results used
- Keep responses natural and conversational
- If needed information wasn't found, explain what you searched for
- Ask follow-up questions if appropriate

Provide your response:"""

        return prompt

    async def _observation_phase(self, step: AgentStep):
        """OBSERVATION PHASE: Process results and update context."""
        self.current_context.state = AgentState.OBSERVING

        # Update working memory with results
        if step.result:
            self.current_context.working_memory[f"step_{step.step_number}_result"] = step.result

        # Add step to plan tracking
        self.current_context.plan.append({
            'step_number': step.step_number,
            'action': step.action.value,
            'tool_name': step.tool_name,
            'success': step.success,
            'reasoning': step.reasoning,
            'timestamp': step.timestamp
        })

    def _should_terminate(self) -> bool:
        """Determine if the agent loop should terminate."""
        if self.current_context.final_response:
            return True

        if self.current_context.current_step >= self.current_context.max_steps:
            return True

        # Check for completion conditions
        if self.current_context.state == AgentState.COMPLETING:
            return True

        return False

    def _build_response(self) -> Dict[str, Any]:
        """Build the final response with loop execution details."""
        context = self.current_context

        return {
            'response': context.final_response or "I wasn't able to generate a complete response.",
            'user_id': context.user_id,
            'timestamp': datetime.now().isoformat(),
            'tool_used': context.tool_results[-1].get('tool_name') if context.tool_results else None,
            'agent_loop': {
                'total_steps': context.current_step,
                'tools_executed': len(context.tool_results),
                'final_confidence': context.confidence,
                'execution_time': time.time(),
                'plan': context.plan
            }
        }

    def _build_error_response(self, error: str) -> Dict[str, Any]:
        """Build error response when loop fails."""
        return {
            'response': f"I'm sorry, I encountered an issue while processing your request: {error}",
            'user_id': self.current_context.user_id if self.current_context else "unknown",
            'timestamp': datetime.now().isoformat(),
            'tool_used': None,
            'error': error,
            'agent_loop': {
                'total_steps': self.current_context.current_step if self.current_context else 0,
                'error_occurred': True
            }
        }

    async def _generate_final_response(self) -> str:
        """Generate a fallback final response if none was created."""
        context = self.current_context

        if context.tool_results:
            # Generate response based on tool results
            tool_summary = "\n".join([
                f"- {result['tool_name']}: {result['result'].get('summary', 'No summary')[:100]}..."
                for result in context.tool_results
            ])

            return f"Based on the information I gathered:\n\n{tool_summary}\n\nIs there anything specific you'd like to know more about?"
        else:
            # Fallback conversational response
            return "I've analyzed your request, but didn't find a need to use any tools. Is there something specific I can help you with or search for?"


# Global instance
_agent_loop = None


def get_agent_loop_service() -> AgentLoopOrchestrator:
    """Get the global agent loop service instance."""
    global _agent_loop
    if _agent_loop is None:
        _agent_loop = AgentLoopOrchestrator()
    return _agent_loop