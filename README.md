# gbs-ConfigLoadSavePlugin
 GBS Plugin that allows configuration of the game saving structure

 NOTE: this plugin and example project was updated for the latest v4.2 dev build (the plugin should still work for 4.1)

New events Save configuration
Allows to specify an array of variable that will be used in the save structure instead of the default one.
Using the save configuration event will create a new save_points.c file containing the specified variables.
If the event is not used, the default structure will be used

![image](https://github.com/user-attachments/assets/dfb79afd-3435-4270-9775-9a7488dca526)
![image](https://github.com/user-attachments/assets/e30eed80-2cc4-4216-850c-c76b6abc2ef0)
![image](https://github.com/user-attachments/assets/df098de2-9883-4bf3-87af-5dd2b4d37134)

New event: Store Variable from Game Data In Variable by Index
If you are using the Save configuration event to have a custom save structure and you want to peek a variable from a save slot, 
use this event instead of the default one.

![image](https://github.com/user-attachments/assets/0790c332-7aca-42fa-a01c-73eb49417610)

The Game Data Load and Game Data Save works just as before with or without a custom save configuration.

Q- When should I use this plugin?

A- If you are using a large part of the SRAM for something else (like using the metatile plugin) this will allows to optimize space for the save data without conflicting with it.
Also useful if you only want to save/load a few variables instead of the whole thing and not act like a weird "save state"
