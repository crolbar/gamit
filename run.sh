#!/usr/bin/env bash

tmux split-window -h ./gamit-server

sleep 0.5

tmux split-window ./gamit
tmux split-window ./gamit
tmux split-window ./gamit
