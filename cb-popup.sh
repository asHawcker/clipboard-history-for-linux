#!/usr/bin/env bash

if command -v clipboard >/dev/null 2>&1; then
    CLIP_CMD="clipboard"
else
    DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
    CLIP_CMD="$DIR/clipboard"
fi

CLIPS=$("$CLIP_CMD" list 2>/dev/null)
EMPTY_STATE=false

if [ -z "$(echo "$CLIPS" | tr -d '[:space:]')" ]; then
    CLIPS="0: [ Clipboard History is Empty ]"
    EMPTY_STATE=true
fi

sleep 0.05

SELECTED=$(echo "$CLIPS" | rofi -dmenu -i -p "Clipboard History" -sync -no-lazy-grab -steal-focus)
EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ] || [ "$EMPTY_STATE" = true ]; then
    exit 0
fi
if [ -n "$SELECTED" ]; then
    CLIP_ID=$(echo "$SELECTED" | awk -F':' '{print $1}' | tr -d '[:space:]')
    "$CLIP_CMD" paste "$CLIP_ID"
fi