"CR3," also known as Control Register 3, holds the physical address of the page table directory base. It is used for context switching, which essentially involves saving the state of a running PE (process executable) and then loading the state of another process. CR3 utilizes something called a page table database (PTD). This contains registers used to map virtual memory to physical memory.

There are many methods to fix CR3; however, the easiest way is brute-forcing. This method is slow and detectable for both external and internal approaches, but I won't go into detail about why it’s detectable, as it’s somewhat pointless in the context of DMA. With DMA, detection isn’t as significant. For DMA, we will use a Virtual Machine Monitor (VMM), which controls virtual machines to interact with the environment (in this case, targeting the virtual memory) to see if we can map the process executable. To achieve this, we need to set up a Descriptor Table Base (DTB), which stores the base address of the PGB (Page Global Directory). The PGB translates virtual addresses to physical addresses. In simple terms, it acts as a pointer that the processor uses for address translation. Brute-forcing involves looping through every possible DTB and configuring the VMM to use each DTB to access physical memory. This approach identifies physical memory regions.

The second method is PTW (Page Table Walk). Here, instead of brute-forcing manually, we iterate through page tables starting from the base address (the virtual address stored in CR3). This process uses a hierarchy of PGD -> PUDP -> PMD -> PLD (Page Global Directory -> Page Upper Directory -> Page Middle Directory -> Page Lower Directory) to map virtual addresses to physical addresses.

The third method involves creating a Pin tool. This tool injects instruments into a running binary, allowing you to gather information about memory accesses, address translations, transactions, and so on. This is the most challenging method by far, requiring the use of a library like DynamoRIO. Note that this can cause compatibility issues with internal cheats, but it is doable.

Although these are the most common methods, other options exist. For example, you could copy the page table and execute the context of the new page table. Another method for internal and external setups is using KPROCESS.DirectoryTableBase, which locates PTDs and provides the CR3. However, this requires creating a thread, as Anti-Cheat mechanisms typically prevent you from directly reading this memory, which results in detection. Alternatively, you could rebuild the PML4 -> PDPT -> PD -> PT structure, but this requires understanding which regions of physical memory are reserved or removed.

It’s worth noting that "CR3" is not entirely the correct term. The focus should be on the Directory Table Base (DTB). CR3 is simply a pointer that directs to the physical mapping. The DTB represents the process of determining the correct value, enabling access to base addresses and modules within a process.

VMM by PCILeech is now limited, requiring you to create a custom version of VMM. I have developed one in Discord that allows for faster DTB translations. They limited this functionality in the main project for a plethora of reasons. This should resolve the issue; however, regarding VMM, you need to change the way it handles transactions. For example, it should only search regions that are relevant. Caching is also effective, and you can implement something like a binary search for ordered data. You can learn more about this from the official PCILeech repo by ULFrisk.

Due to the nature of EAC they are constantly testing things for example they lowered it to 500ms for testing on Rust but it lead to people crashing often so they reverted this change on November 18th after just a couple of hours. Creating a CR3 fix is quite difficult for beginners as there is no procure way of learning how to make it as there is few resources / few sources that actually have it working as they update it frequently.

This repo serves as my implementation of it. It's not the best I made in an hour but for most p2c devs it takes them weeks or a whole new source.

Common questions:

Where's the internal / external?
https://github.com/libalpm64/EAC-Driver the driver has built in CR3 fix by called the function inside EAC (it's not detected yet) I would suggest looking at this implementation for an internal / external.

How do I use this?
.-. If you don't know how to use this then you probally shouldn't be here, I would suggest you go buy instead or hire a developer.
