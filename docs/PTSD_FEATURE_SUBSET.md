# PTSD Feature Subset for Super Meat Boy Style Project

## Purpose

This document defines a practical subset of PTSD features to use for this project.
The goal is to stay focused on platformer gameplay first, then expand only when needed.

## Current Project Status

Based on current app flow:

- State loop is ready: START -> UPDATE -> END
- Core context is used
- Input escape handling is used
- Gameplay objects and renderer tree are not wired yet

## Recommended Feature Subset

## Phase 1: Core Gameplay Loop (Use First)

Use these features immediately:

1. Core::Context
2. Util::Input + Util::Keycode
3. Util::Time
4. Util::Logger

Why:

- Needed for fixed update loop, quit flow, and debug logging
- Lowest complexity and already partially integrated

## Phase 2: Visual Gameplay Objects (MVP)

Add these next:

1. Util::GameObject
2. Util::Renderer
3. Util::Image
4. Util::Animation
5. Util::Transform

What this unlocks:

- Player sprite rendering
- Basic enemy/hazard rendering
- Platform objects with transform and z-order
- Sprite animation state updates

## Phase 3: Player Feedback

Add after movement and collision are stable:

1. Util::SFX
2. Util::BGM
3. Util::Text
4. Util::Color

What this unlocks:

- Jump/death/wall-slide sound effects
- Stage background music
- UI text: timer, death count, debug labels

## Phase 4: Advanced/Optional

Use only if needed:

1. Core::Shader / Core::Program
2. Core::VertexArray / VertexBuffer / IndexBuffer / UniformBuffer
3. Core::Texture / TextureUtils
4. Core::DebugMessageCallback
5. Util::AssetStore (explicit cache management)

What this is for:

- Custom shader effects
- Custom draw pipeline
- Fine-grained rendering and performance control

## Architecture Mapping for This Project

Recommended layering:

1. App (state machine and scene switching)
2. Gameplay layer (Player, Level, Spike, Goal)
3. PTSD Util layer (GameObject, Renderer, Input, Animation, Audio)
4. PTSD Core layer (Context, OpenGL wrappers)

Rule:

- Gameplay classes should mostly depend on Util layer APIs.
- Avoid direct Core OpenGL wrapper usage in gameplay logic unless required.

## Minimum Build Target for First Playable

To reach a first playable prototype, implement in order:

1. Player GameObject with Util::Image
2. Gravity + horizontal movement using Util::Time
3. Input mapping with Util::Input::IsKeyPressed / IsKeyDown / IsKeyUp
4. Static platforms and AABB collision (project-side logic)
5. Camera-like world movement or simple centered stage
6. Death/respawn flow and stage reset
7. Add Util::Animation for player states

## Out of Scope for Early Milestones

Delay these to avoid overengineering:

- Custom shader pipeline work
- Manual buffer management for gameplay sprites
- Complex resource eviction logic

## Quick API Checklist

Use these APIs often:

- Core::Context::GetInstance(), Setup(), Update(), SetExit()
- Util::Input::IsKeyPressed(), IsKeyDown(), IsKeyUp(), IfExit()
- Util::Time::GetDeltaTimeMs()
- Util::GameObject::SetDrawable(), AddChild(), SetVisible(), SetZIndex()
- Util::Renderer::AddChild(), Update()
- Util::Animation::Play(), Pause(), SetInterval(), SetLooping()
- Util::SFX::Play(), Util::BGM::Play()/FadeIn()/FadeOut()

## Integration Note for This Repository

When expanding App:

- Keep App as orchestrator only.
- Move gameplay behavior into dedicated classes under src/.
- Keep rendering tied to GameObject + Renderer graph.

This keeps the project aligned with PTSD's intended architecture.
