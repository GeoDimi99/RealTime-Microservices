#!/bin/bash

# Script to create task folders for spacecraft schedule

echo "============================================"
echo "   SPACECRAFT TASK FOLDER GENERATOR"
echo "============================================"
echo ""

# List of spacecraft tasks
TASKS=("pusl" "satcm" "ssmapl" "ssmapf" "aocscm_pre" "aocn" "aocscm_post" "cmg" "aocn_cmg" "orb" "mhstr" "sfdir")

echo "📁 Creating task folders in task/ directory..."
echo ""

for task in "${TASKS[@]}"; do
    TASK_DIR="task/$task"
    
    if [ -d "$TASK_DIR" ]; then
        echo "⚠️  $task: Folder already exists, skipping..."
    else
        echo "✅ Creating folder: $task/"
        mkdir -p "$TASK_DIR"
        
        # Create app_task.h from template
        cat > "$TASK_DIR/app_task.h" << 'EOF'
#ifndef APP_TASK_H
#define APP_TASK_H

#include <stdio.h>

/**
 * Task: TASK_NAME
 * 
 * TODO: Implement task-specific logic here
 */
static inline void task_main() {
    printf("[TASK_NAME] Task executing...\n");
    
    // ========================================
    // TODO: Add your task implementation here
    // ========================================
    
    // Example: Read inputs, perform calculations, write outputs
    
    printf("[TASK_NAME] Task completed\n");
}

#endif
EOF
        
        # Replace TASK_NAME with actual task name
        TASK_UPPER=$(echo "$task" | tr '[:lower:]' '[:upper:]')
        sed -i "s/TASK_NAME/$TASK_UPPER/g" "$TASK_DIR/app_task.h"
        
        echo "   Created: $TASK_DIR/app_task.h"
    fi
done

echo ""
echo "============================================"
echo "   SUMMARY"
echo "============================================"
echo ""
echo "Created/checked ${#TASKS[@]} task folders:"
for task in "${TASKS[@]}"; do
    echo "  - task/$task/"
done

echo ""
echo "📝 Next steps:"
echo ""
echo "1. Edit each app_task.h file with task-specific logic"
echo "   Example: nano task/satcm/app_task.h"
echo ""
echo "2. Update manifest-spacecraft.yaml to point to new folders:"
echo "   Change 'src: \"sum\"' to 'src: \"<task_name>\"'"
echo ""
echo "3. Build the images:"
echo "   python3 sdk/image-builder/src/main.py -f task/manifest-spacecraft.yaml -c task"
echo ""
echo "4. Deploy and run:"
echo "   docker compose up -d redis deploy-manager"
echo "   docker compose up execution-manager"
echo ""
