#!/bin/bash

set -e

SESSION="chat_test"
tmux kill-session -t $SESSION 2>/dev/null || true

make clean && make
client_exec="./chat_client"
server_exec="./chat_server"

tmux new-session -d -s $SESSION -n "ChatTest" "$server_exec" && sleep 0.1
tmux set-option -g history-limit 10000

names=("TCP_Alice" "TCP_Bob" "TCP_Charlie" "UDP_James" "UDP_Kevin")
for i in {1..5}; do
    if [ $i -le 3 ]; then
        NAME=${names[$i-1]}
        PROTO="tcp"
    else
        NAME=${names[$i-1]}
        PROTO="udp"
    fi
    
    tmux new-window -t $SESSION -d -n "$NAME"
    tmux send-keys -t "$SESSION:$NAME" "$client_exec 127.0.0.1 $NAME 8080 $PROTO" Enter && sleep 0.1
done

# test 0: get help message
tmux send-keys -t "$SESSION:${names[0]}" "/h" Enter && sleep 0.1

# test 1: broadcast message
tmux send-keys -t "$SESSION:${names[0]}" "Hello from TCP Alice!" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[3]}" "Hello from UDP James!" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[4]}" "Hello from UDP Kevin!" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[2]}" "Hello from TCP Charlie!" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[1]}" "Hello from TCP Bob!" Enter && sleep 0.1

# test 2: private message
tmux send-keys -t "$SESSION:${names[3]}" "/d TCP_Bob Hey Bob, I'm using UDP!" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[4]}" "/dm TCP_Charlie Hi Charlie, how are you?" Enter && sleep 0.1

# test 3: change nickname and list
tmux send-keys -t "$SESSION:${names[2]}" "/n TCP_Charlie_New" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[4]}" "/nick TCP_Kevin_New" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[2]}" "/l" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[4]}" "/list" Enter && sleep 0.1

# test 4: send private message to new and old name
tmux send-keys -t "$SESSION:${names[3]}" "/d TCP_Charlie Hi old Charlie, how are you?" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[4]}" "/dm TCP_Charlie_New Hi new Charlie, how are you?" Enter && sleep 0.1

# test 5: quit and list
tmux send-keys -t "$SESSION:${names[0]}" "/q" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[2]}" "/q" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[1]}" "/l" Enter && sleep 0.1

# test 6: send message to inactive user
tmux send-keys -t "$SESSION:${names[1]}" "/dm TCP_Alice Hello?" Enter && sleep 0.1
tmux send-keys -t "$SESSION:${names[3]}" "/dm TCP_Charlie_New Hello?" Enter && sleep 0.1

# last enter session
tmux attach-session -t $SESSION