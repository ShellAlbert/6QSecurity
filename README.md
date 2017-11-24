6QSecurity
This is a security part of 6Q system, Cinema Device Grant Management System. 
All security data was hold in STM32L433RCT6 RAM to avoid cover remove crack.
STM32 runs in Stop2 mode,powered by a 1200mAH battery.Sleep current is low than 5.5uA.
Fours microswitches are covered by a rectangle shell,once one of them is released,this
will trigger STM32 from sleep to run,to clear all RAM data and enter emergency mode.
by zhangshaoyan,shell.albert@gmail.com.
!<6QSM.png>(https://github.com/ShellAlbert/6QSecurity/blob/master/6QSM.jpg)
!<6QBoard.png>(https://github.com/ShellAlbert/6QSecurity/blob/master/6QBoard.jpg)