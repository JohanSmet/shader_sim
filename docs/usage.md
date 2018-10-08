# Using ShaderSim

## Using the command line interface

A shader generally needs quite a bit of input data to run. Specifying these on a command line gets old really quick. For this reason the cli uses JSON formatted files to control the execution of the shader. The format is quite simple and there are only a few commands:

- `associate_data` to set the values of input variables
- `step`: execute one opcode
- `run`: run the entire shader to the end
- `cmp_output`: check the value of an output variable against an expected state.

For more information: check the examples subdirectory of the project.

## Using the browser interface

You can try the browser interface on line [here](https://johansmet.github.io/shader_sim/). Or you can run it locally by starting a web server in the `webui` directory of the project, e.g.:

```bash
cd shader_sim/webui
python -m SimpleHTTPServer    # for Python 2.x
python -m http.server         # for Python 3.x
```

And point your web server at http://localhost:8000/

The interface is pretty straightforward, I hope. You can choose an included shader or upload one of your own. Set the necessary input and uniform variables and step through the shader.