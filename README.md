# edge-detector

A multithreaded image processing program that detects edges in PPM
images using a Laplacian filter.


## Overview
[Edge detection](https://www.mygreatlearning.com/blog/introduction-to-edge-detection/)
is an image processing technique used to find discontinuities in
digital images. This program implements the technique using a
Laplacian filter and parallel processing. Written in C, it
processes one or more P6 PPM images concurrently using threads.
For each input image, the program will create a new P6 image as
output that has the edges of the input image.

```mermaid
graph TD
    A[Start Program] --> B{Check Command<br/>Line Arguments}
    B -->|Invalid| C[Display Usage Error]
    C --> X[Exit]

    B -->|Valid| D[Create Image<br/>Processing Threads]

    subgraph "Per Input File Thread"
        D --> E[Parse PPM File]
        E --> F{Validate<br/>Format}
        F -->|Invalid| G[Display Error]
        G --> Z[Process Next File]

        F -->|Valid| H[Create Worker Threads]

        subgraph "Worker Threads"
            H --> I[Calculate Section<br/>Boundaries]
            I --> J[Apply Laplacian Filter<br/>to Section]
            J --> K[Store Results]
        end

        K --> L[Synchronize Threads]
        L --> M[Write Output File]
    end

    M --> N[Calculate Processing Time]
    N --> O[Display Total Time]
    O --> P[End Program]

%% Styling
style A fill:#0db00f
style X fill:#c30909
style D fill:#e6a111
style H fill:#176de4

%% Add notes for clarity
classDef note fill:#928506,stroke:#666
N1[\"Each input file processed<br/>in parallel"/]
N2[\"Workers process 1/n of<br/>image in parallel"/]

E ---- N1
I ---- N2

class N1,N2 note
```

## Technical Background
### PPM File Format
The P6 PPM (Portable Pixmap) format consists of:

1. Header (in order):

- Image format identifier (P6)
- Optional comments (starting with #)
- Image width and height
- Maximum color value (must be 255)


2. Pixel data:

- Stored in scanline order (left to right, top to bottom)
- Each pixel is 3 bytes (RGB values)

### Laplacian Filter
The program uses convolution to apply this 3x3 Laplacian filter
for edge detection:
```
-1  -1  -1
-1   8  -1
-1  -1  -1
```
