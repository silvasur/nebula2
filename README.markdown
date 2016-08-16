# nebula2

A program for calculating and rendering a [Nebulabrot](http://en.wikipedia.org/wiki/Nebulabrot) image.

## Dependencies / Libraries

nebula2 uses these libraries:

* [SFMT](http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT/index.html) random number generator (included).
* [iniparser](https://github.com/ndevilla/iniparser) for parsing ini files (no, really!). (included as git submodule).
* [pthreads](http://en.wikipedia.org/wiki/Pthreads) for parallelism. Should ship with your Linux distribution.

## Get the sources

### Using git

	git clone https://github.com/silvasur/nebula2.git
	cd nebula2
	git submodule init
	git submodule update

### As `.tar.bz2` archive

[hi-im.laria.me/progs/nebula2.tar.bz2](http://hi-im.laria.me/progs/nebula2.tar.bz2)

## Building

Simply run `make` in this directory.

If you have a CPU that does not support SSE2 you should remove the `-DHAVE_SSE2` part of the `SFMTFLAGS` variable in the Makefile.

## Usage

nebula2 needs a config file. It is an ini file. All parameters must belong to the section \[nebula2\].

### Config parameters

* **width** – The image width.
* **height** – The image heigth.
* **jobsize** – The size of a singe job (how many mandelbrot traces should be recorded during one job).
* **jobs** – The number of jobs to execute. If the image quality is not good enough, you can later increase this number and rerun nebula2. It will continue where it left, if the statefile is still there.
* **threads** – How many threads should be working?
* **statefile** – The current calculation state is saved to this file. This allows you to abort the calculation and continue later.
* **output** – The rendered BMP image is saved to this file.
* **iterX** – The maximum iteration for layer X. X must start with 0 and be in ascending order (i.e. if there is a `iter0` and a `iter2`, `iter2` will be ignored).
* **colorX** – The color for the layer/iteration X. 6 hexadecimal digits `RRGGBB`, where `R` is the red part, `G` the green part and `B` the blue part.

See `example.ini` for an example.

### Aborting and continuing calculation.

Calculation can be aborted by sending the `SIGINT` signal to the nebula2 process (this can usually be achieved by pressing `Ctrl + C` in the terminal with the nebula2 process). Stopping the job might take a while (it needs to save the current state. Also the image is rendered).

You can continue the calculation by simply executing nebula2 with the same config file again (if the statefile is still there).

### Displaying progress

When you send the `SIGUSR1` signal to the nebula2 process, it will display the number of jobs that still need to be calculated. It might stay pretty long at 0 open jobs, since it will render the image then (which can take some time on large images).

You can use this shell snippet to display the progress continuously:

	while true; do kill -SIGUSR1 <PID of nebula2 job>; sleep 1; done

## Portability

Only tested on Linux, might or might not work on other \*nix systems.

Will definetly not work under Windows without some compatibility layer like Cygwin.

## Where is nebula1?

nebula1 is *very ugly* and difficult to use and will therefore not be published. Some code of it is recycled in nebula2, though.