### Two way bash process communication 

This is a code example that allows you to have two way communication with a persistant bash process. I made this because the likes of ```popen``` and ```system``` only allow you to run one command with no variable persistance.

Pros:
- Allows declaring and reading of variables between different commands
- Easily read exit status and output of each command ran
- Can execute long running commands and will return when it has finished

Cons:
- More overhead than ```popen``` and ```system```

For the full explanation, read this [post](https://www.danielshare.co.uk/how-to-send-multiple-commands-and-receive-output-from-the-same-bash-process).

## How to build

Compile with: ```g++ -std=c++17 main.cpp -o main```