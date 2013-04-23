1. Download and install Instant Contiki http://sourceforge.net/projects/contiki/files/Instant%20Contiki/

2. Open it (Can be done in virtualbox) (password is "user")

3. Open a terminal and execute

 

cd ~

git clone http://<USERNAME>@bitbucket.org/MBradbury/cs407-group-project.git

rm -rf contiki contiki-2.6

ln -s /home/user/cs407-group-project/contiki /home/user/contiki

ln -s /home/user/cs407-group-project/contiki /home/user/contiki-2.6

cd cs407-group-project

git submodule init && git submodule update

cd contiki

git pull

cd ../

 

 

4.

4.a. Edit ~/.bashrc to contain the line "export CS407DIR=/home/user/cs407-group-project"

4.b. Restart the vm

4.c. Check that this step worked by doing "cd $CS407DIR". If you have future issues this may be the problem

 

5.

Open the following files:

 - $CS407DIR/Algorithms/TDMA/tdma.c

 - $CS407DIR/Algorithms/TDMA/Makefile

 - $CS407DIR/Tests/COOJA-Tests/gatherResults.js

 

Please do not commit the modifications you will be making to these files!

 

6.

 

Every test has a certain number of parameters:

 A The predicate checking algorithm being used

 B The predicate being checked

 C The network size

 D The predicate check period

 

To configure A:

You need to edit the TDMA makefile and change the PRED_TYPE value to be that of the

predicate type you are checking. Edit this now.

 

Valid entries: PELP PELE PEGP PEGE

 

To configure B:

(Will consider this if there is time)

 

Valid entries: 1HOP

 

To configure C:

When opening Cooja (using the shortcut on the desktop) you need to enter this value.

You will configure this later

 

Valid entries: 15 30 48

 

To configure D:

You need to edit line 127 of tdma.c that you opened.

 

Valid entries: 4.0 2.0 6.0

 

(Only do this if there is time, default to 4.0)

 

 

--Example Parameters--

--A=PELP

--B=1HOP (just include this in the directory paths mentioned next)

--C=15

--D=4.0 (Leave as is for now)

 

So at the moment there are only 2 parameters to change A and C.

A is set by editing the makefile and C is set when opening Cooja

 

7.

 

Once you know what parameter combination you are running for

you need to create the directory structure for it.

 

mkdir $CS407DIR/Results/TDMA/<A>

mkdir $CS407DIR/Results/TDMA/<A>/<B>

mkdir $CS407DIR/Results/TDMA/<A>/<B>/<C>

mkdir $CS407DIR/Results/TDMA/<A>/<B>/<C>/<D>

 

You must create the directory structure using the valid entries specified in 6

 

Once the directory structure is created you need to modify gatherResults.js

Line 13 has the output directory, you should make it equal to:

 

"/home/user/cs407-group-project/Results/TDMA/<A>/<B>/<C>/<D>/"

 

 

8. Now we need to setup the cooja script. Open cooja by double clicking the icon on the desktop.

 

8.a. Press File -> New Simulation

8.b. Tick the box that says "new random seed on reload"

8.c. press create

8.d. Press Motes -> Add motes -> Create new mote type -> Sky Mote

8.e. Press browse

8.f. Navigate to /home/user/cs407-group-project/Algorithms/TDMA

8.g. Select tdma.c and press open

8.h. Press clean ***ALWAYS MAKE SURE TO CLEAN WHEN CHANGING PARAMETERS, clean on its own is not enough, so you will need to go the the TDMA directory and delete the obj_sky directory and the symbols.* and tdma.sky files***

8.i. Press compile

8.j. Press create

8.k. Set the number of motes to be the value <C>

8.l. Set the positioning to be "Linear"

8.m. Change the 100s in the text boxes to [150 for <C> == 15] or [200 for <C> == 30] and [300 for <C> == 48]

8.n. Press Add motes

8.o. Press Tools -> Simulation Script Editor

8.p. Highlight the code in gatherResults.js and copy it.

8.q. Highlight and delete the code in the editor

8.r. Copy the gatherResults.js code in

 

To Test what you have done so far:

8.s. Press Run -> Save simulation and run with script.

If there are any issues you will need to go to "$CS407DIR/Results/TDMA/<A>/<B>/<C>/<D>"

and remove any 0 byte files. *****Make sure you have set the directory correct in gatherResults.js or

you may contaminate results with other parameters*****

 

To prepare to run:

 

8.s. Press Run -> Activate

8.t. In the main COOJA window Press File -> Save Simulation as...

8.u. Navigate to /home/user

8.v. Save simulation as TDMA.csc

8.w. Repeat steps 8.t. to 8.v. but instead save the simulation to "$CS407DIR/Results/TDMA/<A>/<B>/<C>/TDMA.csc"

8.x. Exit out of COOJA

 

***Before you save a simulation make sure the time is 00:00.00***

***BEWARE pressing CTRL+S will cause the simulation to start, if that happens press pause and reload***

 

9.

 

In a terminal navigate to $CS407DIR/Results and execute "python run.py"

once that finishes repeat steps again with different parameters

***making sure to clean tdma properly***.