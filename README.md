# SRIP_Hrtf_Spatial_Audio_Project <hr>
<h1>Background</h1>
This program is part of Summer Research Internship Program by Eletrical Computing Engineering Department at UC-San Diego.
<a href="https://sol0092.wixsite.com/website" target="_blank">Summer Research Internship Program Official Website</a> <br> </p>

<H1>Abstract</h1>
As of now, there are not many online tools available to learn about spatial audio, and almost no demos that experiment with personalization of head-related transfer function (HRTF). HRTF describes how a sound wave is affected by the head and body as it travels through space, so personalizing it to an individual’s anthropometric measurements can improve sound localization. The CIPIC database contains ear measurements and corresponding HRTFs of 45 people that can be used by researchers to find an individual’s closest HRTF match. Previous HRTF matching techniques have extracted distances from ear pictures, relying mostly on ear size. Our Matlab application offers three algorithms that provide a match based on ear shape. The user can select from block segmentation with Hu moment invariants, principal component analysis, and Q-vector analysis. After the closest ear shape match from the CIPIC database is identified, the corresponding HRTF is used in our demo (instead of the standard MIT KEMAR model HRTF). The demo was created by building on a Github program of a sound moving 360 degrees horizontally. We used C language with Simple DirectMedia Layer 2 libraries, creating a layout for the user to specify the azimuth of the sound that they are hearing. We are creating an easily accessible, streamlined educational module for users to learn about spatial audio, and test for themselves whether the localization of the audio is improved. <br>

<h1>Team</h1>
Academic Advisor: Professor Truong Nguyen <br>
Software Development: Branson Beihl, Songyu Lu 
<hr>
Credit to Ryan Huffman's hrtf-spatial-audio project, which this project was based upon. 


