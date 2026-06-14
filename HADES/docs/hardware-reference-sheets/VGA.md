# VGA Controller

Video output controller that renders pixel/character data to a display. Turns CPU execution into visible output.

## Data Path

```
CPU → Memory (framebuffer) → VGA Controller → Display
```

## Display Parameters

| Parameter | Typical Value |
|-----------|---------------|
| Resolution | 640 × 480 |
| Pixel clock | 25 MHz |
| Refresh | 60 Hz |

## Pixel Formats

| Format | Bits |
|--------|------|
| RGB565 | 16-bit |
| RGB888 | 24-bit |
| RGB101010 | 30-bit |

HADES uses: `uint32_t color = 0xRRGGBB;`

## Framebuffer

Linear memory layout (row by row):
```
address = y * WIDTH + x
```

```cpp
const int W = 640;
const int H = 480;
uint32_t framebuffer[W * H];
```

## Pixel Write

```cpp
void put_pixel(int x, int y, uint32_t color) {
    framebuffer[y * W + x] = color;
}
```

CPU writes to framebuffer via memory-mapped access:
```cpp
memory[FRAMEBUFFER_ADDR + offset] = color;
```

## Video Pipeline Components

| Component | Function |
|-----------|----------|
| Pixel Buffer DMA | Reads frame from memory, sends to VGA |
| Character Buffer | Renders ASCII text → pixels |
| FIFO | Synchronizes clock domains |

HADES simplification: framebuffer → directly rendered (no DMA/FIFO simulation needed).

## Character Display

Text rendering via character buffer:
```cpp
draw_text("Hello", x, y);
```

Useful for printf-style debugging and output visualization.

## HADES Integration

The VGA module makes memory writes observable:
```
CPU store → cache → memory → framebuffer → visible on screen
```

## Security Relevance

- **Visual side-channel**: Internal state displayed → leaked
- **Debug leakage**: Sensitive data printed → visible
- **Covert channel**: Process A draws pixels, process B observes timing
- **Timing correlation**: Frame updates tied to execution timing
