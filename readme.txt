File: readme.txt
Author: Gerald Kang
----------------------

1.
One design decision I made was to represent my header at the front 8 bytes but have my payload not include the header. Thereofore, the size stored in my header would not include the header. This was because it allowed me to better visualize the blocks within and implenet the right jumps to the next block.
Another design decision I made was I added the coalescing multiple blocks insetad of just one. I did this by implementing a function that naturally links to free blocks together, update_free, which allowed me to then use this newly unliked space. Because I was able to unlink each block successfully, I was able to implement a while loop that iterated through all following free blocks, not just one, and unlink them as I go.
My allocator would strow strong performance in reallocating as it is able to reallocate in place even if the free block is stopped by the segment end. However, my allocator would show decreased performance on repeat because mymalloc function does not have this implementation, causing it to malloc the new memory block after nused bytes instead of the existing free block.
One attempt I made at increasing optimization was going through and reducing multiple line calculations and tried to simplify them if I could. By simplify I mean perform the smallest calculation that can be used for bigger calculations later on to improve the registry.
2.
One assumption mymalloc makes is that the inputted pointer is a valid one and at the right location. If a user inputted a wrong ptr or one not pointing directly at the first byte, then the sizing would get off leading an offset when trying to calculate headers and sizes leading to a crash.
Another assumption my free makes is that the inputted pointer is pointed at the right location. If it was put in another location, then I would not properly mark the ehader as empty as in myfree, I free the memory location 8 bytes before. So if the user were to give me a pointer 3 bytes in, this would lead to the program not ending up at the right header causing a bug.
Tell us about your quarter in CS107!
-----------------------------------
It was fun! I took this class basically alone so I was pretty afraid but It went a lot better than I expected which I am glad. I'm super proud of the assignments I've been able to complete and what I can now do and I just wanted to say thank you so much! I really do feel like I learned a lot.


