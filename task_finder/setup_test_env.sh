#!/usr/bin/env bash
# Skript pro vytvoření čistě lokálního testovacího Git repozitáře (100% offline, bez internetu)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_REPO="$SCRIPT_DIR/test_repo"

echo "[1/6] Creating test folder: $TEST_REPO"
rm -rf "$TEST_REPO"
mkdir -p "$TEST_REPO"

# Initialize git repository
git -C "$TEST_REPO" init -b master
git -C "$TEST_REPO" config user.name "Tester"
git -C "$TEST_REPO" config user.email "tester@example.com"
git -C "$TEST_REPO" remote add origin "https://github.com/Type3Games/horror.git"

echo "[2/6] Creating branch master..."
cat << 'EOF' > "$TEST_REPO/TASK.md"
# NAME
Base Project Configuration
# STATUS
dormant
EOF
git -C "$TEST_REPO" add TASK.md
git -C "$TEST_REPO" commit -m "init master"

echo "[3/6] Creating branch task/monster-ai (Pepa as Task Master without subtasks -> Pepa's active task)..."
git -C "$TEST_REPO" checkout -b task/monster-ai
cat << 'EOF' > "$TEST_REPO/TASK.md"
# NAME
Implement Monster AI
# DESCRIPTION
Monster pathfinding and chase behaviors in corridors.
# TASK MASTER
Pepa
# STATUS
available
# DEADLINE
26/09/20
# SUBTASKS
EOF
git -C "$TEST_REPO" add TASK.md
git -C "$TEST_REPO" commit -m "add monster ai task"

echo "[4/6] Creating branch task/inventory (Jirka as worker -> Jirka's active task)..."
git -C "$TEST_REPO" checkout -b task/inventory
cat << 'EOF' > "$TEST_REPO/TASK.md"
# NAME
Player Inventory System
# DESCRIPTION
Key collection and flashlight battery inventory slots.
# TASK MASTER
Honza
# WORKER
Jirka
# STATUS
available
# DEADLINE
26/09/25
# SUBTASKS
[UI layout](https://github.com/Type3Games/horror/tree/task/inventory-ui)
EOF
git -C "$TEST_REPO" add TASK.md
git -C "$TEST_REPO" commit -m "add inventory task"

echo "[5/6] Creating branches with OPEN tasks..."

# Open WORKER (Honza is TM, worker slot is open: ---)
git -C "$TEST_REPO" checkout -b task/audio-system master
cat << 'EOF' > "$TEST_REPO/TASK.md"
# NAME
3D Spatial Audio System
# DESCRIPTION
Footstep sounds and environmental reverberation.
# TASK MASTER
Honza
# WORKER
---
# STATUS
available
# DEADLINE
26/09/14
EOF
git -C "$TEST_REPO" add TASK.md
git -C "$TEST_REPO" commit -m "add audio task"

# Open TASK MASTER (TM slot is empty)
git -C "$TEST_REPO" checkout -b task/level-design master
cat << 'EOF' > "$TEST_REPO/TASK.md"
# NAME
Level Design - Escape Corridor
# DESCRIPTION
Geometry modeling and collision meshes for escape hallway.
# TASK MASTER

# STATUS
available
# DEADLINE
26/09/28
EOF
git -C "$TEST_REPO" add TASK.md
git -C "$TEST_REPO" commit -m "add level design task"

# Urgent task (closest deadline -> highest score) - both TM and WORKER open
git -C "$TEST_REPO" checkout -b task/cutscene-intro master
cat << 'EOF' > "$TEST_REPO/TASK.md"
# NAME
Intro Cutscene in Ambulance
# DESCRIPTION
Scripting the intro wakeup sequence.
# TASK MASTER

# WORKER
---
# STATUS
available
# DEADLINE
26/09/11
EOF
git -C "$TEST_REPO" add TASK.md
git -C "$TEST_REPO" commit -m "add cutscene task"

# Inactive / dormant task (should not be offered)
git -C "$TEST_REPO" checkout -b task/multiplayer-proto master
cat << 'EOF' > "$TEST_REPO/TASK.md"
# NAME
Multiplayer Prototype
# TASK MASTER

# STATUS
dormant
# DEADLINE
27/01/01
EOF
git -C "$TEST_REPO" add TASK.md
git -C "$TEST_REPO" commit -m "add dormant task"

# Switch back to master
git -C "$TEST_REPO" checkout master

echo "============================================================"
echo "DONE! Offline test Git repository created in:"
echo "$TEST_REPO"
echo "Branches:"
git -C "$TEST_REPO" branch
echo "============================================================"
