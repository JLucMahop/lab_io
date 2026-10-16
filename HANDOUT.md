# Lab 2. Who moves the bytes?

**25 minutes, in pairs.**

Part I of the course claimed that a CPU moving data itself cannot keep up,
and that is why devices were given a DMA engine. Here you measure it: the
same 64 MiB write to the same disk, twice, with the transfer mode changed
underneath.

## Running it

```
./run-io-lab.sh
```

The script boots a VM twice. The disk is the same file both times; only the
kernel command line differs:

| mode | kernel option | who moves the bytes |
|---|---|---|
| DMA | *(none)* | the disk controller's DMA engine |
| PIO | `libata.force=pio0` | the CPU, one word at a time |

Each run prints throughput, CPU time, and CPU time per MiB. The script ends
with a comparison table.

---

## Q1. Read the two runs (5 min)

Write down, for each mode: throughput, ms of CPU per MiB, and the percentage
of the wall time the CPU was busy.

- Which number changes the most between the two modes?
- In PIO mode, what is the CPU actually doing during those seconds?

## Q2.  Cycles per byte (10 min)

Your VM's clock speed is in `/proc/cpuinfo` on the host.

1. Compute the CPU cycles spent per byte written, in each mode.
2. In PIO mode, a word is moved per `in`/`out` instruction. Does your figure
   match the number of instructions you would expect? What accounts for the
   rest?
3. In DMA mode the CPU still spends *some* time. On what?

## Q3. Back to the per-packet budget (5 min, on paper)

The course slide gave a 10 Gb/s NIC a budget of **1.2 µs per 1500-byte
frame**, and 120 ns at 100 Gb/s.

1. Using your measured PIO cost per byte, how long would the CPU need to move
   one 1500-byte frame?
2. How does that compare to the budget? What line rate could PIO sustain?
3. Step 8 of the DMA walkthrough still has the driver copy the DMA buffer
   into the destination buffer. Using your DMA-mode numbers, what does that
   copy cost per frame? Why do people work so hard on zero-copy?

## Q4. Why does the CPU column lie? (5 min)

Look again at the two runs. The throughput collapsed in PIO mode, but the
*guest* CPU time per MiB barely moved (it may even be lower).

- In PIO mode, what happens on every single port access, and is the guest
  CPU running during it?
- Which column of the comparison table shows the cost instead?
- What does this tell you about trusting `getrusage()` inside a VM?

## Q5. Where did the wall time go? (10 min)

The comparison table has two more columns: **VM exits** and **PIO exits**,
counted on the host with `perf`.

1. How many exits did each mode produce? What is the ratio?
2. You wrote 64 MiB in PIO mode. Divide the PIO exit count by the number of
   bytes: how many bytes travel per exit? Does that match what an `out`
   instruction moves?
3. Take the extra wall time of the PIO run and divide it by the extra number
   of exits. What is the cost of one exit? 
4. In DMA mode, exits still happen, but far fewer. What triggers them?

## Q6. What did we actually change? (5 min)

`libata.force=pio0` changes neither the disk nor the file nor the program.

- What exactly did it change?
- Nothing in this lab mentioned an IOMMU. In DMA mode, which physical
  addresses did the controller write to, and who chose them?
