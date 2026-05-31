# May 1st, 2026: First day, started CAD !

I spent this first session doing some of the CAD for the slider assembly, this is pretty complex and I want it to be modular so I learnt a lot about global variables in solidworks, so that each parameter can be adjusted freely.
I spent a lot of time sketching for this project during class (yes, I know that I should be paying attention but philosophy is boring), so I had a pretty good idea of what I wanted to do, I looked at some existing camera sliders and either the camera couldn't rotate, or it was with an additional tool, so my idea was to combine everything into one, using of the shelf parts, instead of carbon fiber rods or weird unfindable aluminium profiles, I use simple 4040 extrusions, with double v-slots, my design is kinda inspired by eldekrone's sliderplus-v6 in the movement aspect, as it is the only one I have seen that can travel almost 2 times it's own lenght (only on a tripod) by being mounted on a moving carriage, that goes in the opposite direction as the camera.

<img width="639" height="457" alt="image" src="https://github.com/user-attachments/assets/21f9076f-9794-458e-9014-dd2902f2ea50" />

**Total time spent: 2h17**

# May 2nd, 2026: Added feets !

In this second session I expanded on my previous design session, I made the early sketch of the under carriage that will get mounted on the tripod, and I made feets that house an M5 bolt with an embedded M5 nut, I modified the end caps accordingly to allow for those feets to screw in, and give a flexible way to adjust the slider's level.
I also took the time to adjust the main carriage's height, because it was a bit too high, and added some groves for the wheels to fit with that lowered part.

<img width="885" height="512" alt="image" src="https://github.com/user-attachments/assets/5ac0d1b4-54aa-46ca-a9ab-61eb265a3e3a" />

**Total time spent: 2h9**

# May 3rd, 2026: Still doing CAD !

For this short third session, I started with journaling the second session, then I did some CAD work, I added slots to insert locknuts for the v slot wheels to attach to the undercarriage.
I also did some research on how tripod screws work, so that I can attach my slider to a tripod, I want it to be extremely secure, as when it is fully extended, the forces are quite big, so I decided to go with a really long nut, a coupling nut, with a bolt on the other side to make sure it doesn't go anywhere.
I wondered about adding bolts that go through the under carriage, like in the camera carriage, to either adjust the wheels, and add a lot of strenght, but I decided against it for now, in my inspiration, the edelkrone sliderplus v6, they are present, but I am not sure if they are absolutely necessary, and I want to reduce bloat and hardware.
If I do come to regret this, I can just make another version and 3d print it again, and maybe even try a cf or gf reinforced filament.

<img width="692" height="356" alt="image" src="https://github.com/user-attachments/assets/946110cd-f363-466c-9990-d593df3153b7" />

**Total time spent: 1h13**

# May 4th, 2026: Movement, belts, the whole enchilada !

I just finished session 4, today I worked on the belt system.
I was kind of afraid of starting to work on it but as always, it wasn't too bad.
I managed to find 3d models for my stepper motors, timing and idler pulleys, both at gt2 20T 6mm, which I guess is the most common, used for 3d printers and such.
then I made a mount on one of my end caps for the motors, then for the idler pulleys. I think I will have to reinforce the different parts later on, and clean up the geometry to make it look good.

<img width="1162" height="543" alt="image" src="https://github.com/user-attachments/assets/f035bf8f-b86f-436f-b927-71e6741611f7" />

**Total time spent: 1h45**

# May 5th, 2026: Attaching the timing belts

During session 5, I added another idler pulley at the bottom, and started to work on the mounting system between the sliders and the timing belts.

<img width="671" height="451" alt="image" src="https://github.com/user-attachments/assets/d1fbfd5a-f125-4775-8bb2-49055d85f979" />

**Total time spent: 1h06**

# May 6th, 2026: Attaching the timing belts again (with the bottom this time)

In this 6th session I added the slot for the GT2 6mm timing belt, and I added an attachment point for the undercarriage as well.

<img width="1207" height="424" alt="image" src="https://github.com/user-attachments/assets/700c3146-7007-4406-a709-27b5ece6b9ac" />

**Total time spent: 1h02**

# May 7th, 2026: I cooked some CAD again

In this seventh session, I designed a whole bunch of stuff :
- added/changed the rotation belt path
- added the rotation mechanism
- made the camera platform, with an added dovetail to adjust the center of mass of the camera
- made the dovetail mount
- solved a lot of small misc issues

<img width="1332" height="846" alt="image" src="https://github.com/user-attachments/assets/19f2e7f4-4e1c-460c-8a85-4770d6ae42ea" />

**Total time spent: 5h02**

# May 8th, 2026: Stablizing the platform, and preparing pcb space !

During this eighth session, I added 2 bearings for stabilization under the camera platform, I then added an endstop where I will try to fit all of the electronics.
I added groves for the future PCB.
Then I added recesses for the motor wires, with little slots for cable holders.
After that was done I modeled a case for the battery, with a dovetail, and I added the opposite dovetail to the left end cap to attach it.
Seeing all of my progress, I started to work on the aestetics of my build, by adding a lot of fillets on all of the parts, and sourced 4 screws that I forgot while designing (I almost payed 10$ for 4, but I managed to find some for 5$) and I also sourced a usb-c port, and I added a slot for it on my design.

<img width="1135" height="561" alt="image" src="https://github.com/user-attachments/assets/42397116-7536-4ed0-aba5-d7643423e3cb" />
<img width="575" height="652" alt="image" src="https://github.com/user-attachments/assets/3c46a534-5d2e-4128-bedf-776b8b4fb344" />
<img width="936" height="577" alt="image" src="https://github.com/user-attachments/assets/bde194f6-7bdd-47f5-8506-067f71229eef" />
<img width="658" height="568" alt="image" src="https://github.com/user-attachments/assets/4c8d9495-dca2-4081-b0d7-0de76eb14b2e" />

**Total time spent: 2h28**

# May 9th, 2026: Finished the CAD (1st version at least) !

In this ninth session, I pretty much finished up the CAD, did some measurements to get the lenght of GT2 belts I need, and then I moved on to the PCB, I managed to find the symbols I needed, and made a footprint for the esp.
I'm quite scared as it is going really fast, and I'm only at around the 20hr mark, I know I have some stuff left to do but I counted almost everything I need and I'm at around 170€, or exactly 200$. which is 40hrs of work, the cheapest aluminium extrusions I found are 70€ total, with shipping, and the rest of the parts add up to 100€ from aliexpress.

<img width="721" height="334" alt="image" src="https://github.com/user-attachments/assets/15a1571e-53da-459a-a81b-59e8a643fc7f" />

**Total time spent: 1h08**

# May 10th, 2026: Doing the PCB ! 

During this tenth session, I almost finished the PCB, I am still missing the routing but all of the rest is done.
I made a lid for the pcb and electronics in the cad, and I added a hole that will have a pin, used to lock the carriages together for transport.

<img width="904" height="539" alt="image" src="https://github.com/user-attachments/assets/1e2950f3-09bc-49e6-bdfd-c8257cfd613c" />

**Total time spent: 1h47**

# May 11th, 2026: Finishing the PCB !

in this eleventh session I continued on my previous work, I finished the pcb layout and did the routing, then I finished the pin that can lock both carriages together for transport, and added magnets at the end so it's secure, and made a small storage hole for it, then I made the belts that allow the movement, and when they were done I realised that they were interfering with the usb c port location, so I moved that.

<img width="1232" height="267" alt="image" src="https://github.com/user-attachments/assets/38b27b52-5434-4761-9881-00a064bfda4b" />
<img width="1156" height="703" alt="image" src="https://github.com/user-attachments/assets/ccb2f3ae-6efc-411e-b9de-b6d62fc020fc" />

**Total time spent: 1h40**

# May 12th, 2026: Starting the controller !

This is the 12th session I believe, today I started research on the controller that I want to design, and maybe make if the budget allows, for the camera slider.
I used google and AI to do some preliminary research, while always reviewing it's claims with datasheets, forums, and common sense.
I started to source the different parts that I need, and started work on the PCB, it has a lot in common with the hackpad that I made, so I took some parts from it, but sadly this is very fast, so the plan of taking some time to design a cheap controller to have more budget for the slider isn't really going to plan...
I think the final steps of doing the github, bom, exporting everything, the zine, and most importantly the code, are vastly underestimated by myself, so there is hope that I reach the ~45-60hrs I need to finance it.
(now that I think about it, I have to code dialog between 3 esp's, and a phone, over wifi, and also make a cool interface for the website, and another one for the controller, and also code every feature by hand...)

<img width="900" height="807" alt="image" src="https://github.com/user-attachments/assets/eaba06d1-4a4f-4776-9949-d3e56963c5e5" />

**Total time spent: 1h16**

# May 13th, 2026: Research for the controller !

Today I did more research and planning and pcb for the controller, my previous plan kinda went overboard when I realized that the parts I was looking for kinda weren't real.
So I switched my approach to use joysticks instead, just like a drone controller.

<img width="1171" height="805" alt="image" src="https://github.com/user-attachments/assets/b07ebaff-1adc-48be-952a-3e6cac2e254b" />

**Total time spent: 1h11**

# May 14th, 2026: Controller PCB 

Hey, during this short 14th session I worked on the controller pcb again, I realized that pcb costs were really high for larger than 100*100mm pcb's, so I thought of a clever plan, doing 2 different pcb's, and connecting them with cables, and the second pcb will be present twice in the final build, because I get 5 per order anyway.

<img width="825" height="458" alt="image" src="https://github.com/user-attachments/assets/495840f3-097f-4218-a283-10bfcd2cdb90" />

**Total time spent: 2h17**

# May 15th, 2026: Moved some PCB stuff around because of KiCAD

Session 15, here we go !
I continued working on the controller for the slider, I spend more time that I would like to admit on a problem, that is a limitation of kicad, every pcb has to have a project file, you can't really have 2 pcb's in a single project file.
So I had to make a new project file for the key pcb's, and modify the old one to remove the keys, so that took a while.
Then I started to work on the CAD for the controller, I exported the pcb's in step and made a sketch for the controller's over all shape. That's it for today, I don't really know how I can make it ergonomic in solidworks, but I'll try anyway. Oh and if I don't see ya, good afternoon, good evening and goodnight !

<img width="1060" height="759" alt="image" src="https://github.com/user-attachments/assets/cc1c93ab-87ce-440e-9821-86e3a54a968f" />

**Total time spent: 1h03**

# May 16th, 2026: Bday + Controller CAD

Session 16 ! today I celebrated my birthday, yay ! but that also means I didn't have much time to work...
Today I continued on my controller journey, I modified a bit the pcb's because they were incorrect, then I made all of the stands for the pcb's, after that was done, I found a 3d model of the screen I'm using, so I added that, and also made little standoffs so that the screen doesn't flex.
In the end I started to try and make an ergonomic shape for the controller, but that proved to be kinda hard...

<img width="1363" height="804" alt="image" src="https://github.com/user-attachments/assets/71dd3634-4aa9-4ffc-875e-554d3832b7b6" />

**Total time spent: 1h09**

# May 17th, 2026: Ergonomics is hard ! :pf:

Session 17... this one was a bad one. I spent the whole hour fighting against solidworks to try and make ergonomic handles for the controller, they still don't look really good, but I will have to try them to see how they can be improved.

<img width="651" height="354" alt="image" src="https://github.com/user-attachments/assets/a5a3a4bc-241a-4597-b396-7197cea80522" />

**Total time spent: 1h07**

# May 18th, 2026: Solidworks recovery, and added keycaps !

Today started on a bad note... on top of the site being down my solidworks controller assembly wouldn't open, and when it did, all of the dependencies had to be replaced, that meant also replacing every constraint :pf:.
But apart from that, I managed to make a little progress, added the keycaps, made holes for the pcb pins to have room, and for the overall controller to be slimmer, next time I can start on the top cover.

<img width="1160" height="720" alt="image" src="https://github.com/user-attachments/assets/a4b7ded8-a3ae-4875-b2c2-dc5738d801ad" />

**Total time spent: 1h05**

# May 19th, 2026: Solidworks is still mad at me !

The nineteenth session started as usual with this file, with all the step files being reverted, and all of the constraints being removed, after quickly fixing that, I started work on the top part, and pretty much finished it, apart from some art that I want to make for it.
(also, it looks like a happy frog when upside down!)

<img width="1213" height="825" alt="image" src="https://github.com/user-attachments/assets/0f4e8b57-0ec9-4b21-b5e3-670606a742ed" />

**Total time spent: 1h17**

# May 20th, 2026: I'm also an artist !

20th session, :YAY: today I did some art, it's pretty mediocre but it's still art.
I made a frog sticker that goes on the controller, because I think it looks like a frog when upside down, then I made some changes to the github page, I'm still ways to the end, but I need to keep pushing, next sessions will probably be short as well, because of school stuff, but next week I have a 4 day weekend all alone, I think I can do my own hackaton and cram as many hours as possible !

<img width="1182" height="830" alt="image" src="https://github.com/user-attachments/assets/2fa82bf1-7271-4a01-98b4-723ff5cf5379" />

**Total time spent: 1h04**

# May 21st, 2026: Boring BOM stuff

Today I worked on my BOM, and started to realise how much this project was actually gonna cost (ouch).
I also started to think about the camera head, and what I need to buy for it. (in the end, I decided to split my project, because this is too ambitious for a single project to do both, and the opensauce budget is fixed)

<img width="1605" height="864" alt="image" src="https://github.com/user-attachments/assets/df1238c7-f045-4abe-90db-3f92e5e8caef" />

**Total time spent: 1h07**

