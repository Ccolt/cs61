#   Section 5 Assignment Solutions
---

##  Memset

### Control Runs

In 1-6, note that running `t1` first vs running `t2` first produces different results. Why is this? (The Prefetching part at the end of Section 5's notes briefly touch upon this, though this is not a required part of the class.)

To control for this behavior, we run `t1` again after running `t2`; and just to make sure there's no chicanery, we also run `t2` again after `t1`. So, our run would look something like

1.  `t1`
2.  `t2`
3.  `t1`
4.  `t2`

We verify the lack of chicanery by checking to that the times of 2 and 4 are roughly the same, and then compare the times in runs 2 and 3 to get the ratio of `t2`:`t1`. In the Solutions below, we first present the ratio of runs 2 and 1, then presented the controlled ratios of runs 2 to 3.

### Solutions

1.  For N &leq; 1000000 (1 million), the ratio of `t2` to `t1` is roughly 1:1.3, or ~1:1.02 (controlled). Running `t2` first results in roughly 1:0.75 or so.
2.  For N &geq; 10000000 (10 million), the ratio of `t2` to `t1` is roughly 1:1.03, or ~1:1 (controlled).
3.  As N gets larger, the cost of moving the elements (i.e. memory accesses) outstrips the cost of the loop. As a result, the performance benefits of loop unrolling shrink as the number of elements increase.
4.  For N &leq; 1000000 (1 million), the ratio of `t3` to `t2` is roughly 1:9, or 1:7 (controlled). Running `t3` first results in roughly 1:5.5 or so.

    For N &asymp; 5000000 (5 million), the ratio of `t3` to `t2` is roughly 1:4.5, or ~1:3.5 (controlled). Running `t3` first results in roughly 1:3 or so.

    For N &asymp; 10000000 (10 million), the ratio of `t3` to `t2` is roughly 1:2.5, or ~1:25 (controlled). Running `t3` first results in roughly 1:2.5 or so, still.
5.  Accounting for control, for N &asymp; 16384 (2<sup>14</sup>) yields `t1`:`t2`:`t3`:`t4` is roughly 4.73:4.8:0.62:0.61. These are fairly unstable. Finding a good N for `t1`-first (without controlling) is quite difficult.
6. To remove the `n % 8 == 0` and `n % 64 == 0` requirements from `t2`, `t3`, and `t4`, we can add another loop before returning:

```
// Main loop begins
// ...
// Main loop ends
// Deal with remaining elements
while (p < endp) {
    *p = c;
    p++;
}
return s;
```

To remove the `((uintptr_t) s) % 8 == 0` requirement from `t3` and `t4`, we can also add a loop before the main loop (in conjunction with the previous change):

```
while (((uintptr_t) p) % 8) {
    *p = c;
    p++;
}
// Main loop begins
// ...
// Main loop ends
while (...) {
```

## Traverse

### Control Runs

As with `memset`, we find that the first run typically runs slower, especially for small N. Again, we will show both `t0`-first and controlled results.

### Solutions

7.  For 1000000 (1 million) &leq; N &leq; 10000000(10 million), the ratio of `t0` to `t1` is roughly 1:1.03, both controlled and `t0`-first.
8.  For N &asymp; 700000 (700 thousand), the ratio of `t0` to `t1` is roughly 1:1, or ~1:1.02 (controlled).

    For N &asymp; 75000 (75 thousand), the ratio of `t0` to `t1` is roughly 1:0.375, or ~1:1 (controlled).
9. These results seem to suggest that the array implementation is more efficient for larger N, and the dynamically-allocated-per-node implementation is more efficient for smaller N; however, for large N, the array implementation isn't *significantly* faster; it's more that the dynamically-allocated-per-node implementation is significantly slower.

## Advanced: Prefetching

* For large N, prefetching helps the `list` implementation perform comparably to the prefetched `array` implementation; that is, it helps `list` a bit more than it helps `array`. For small N, there's no visible difference.
* For large N, prefetching helps the `array` implementation a bit, but not as much as `list`. For small N, prefetching may actually cause `array` to slow down!
* We have seen that, for small N, prefetching sometimes hinders the `array` implementation. We can attribute to the fact that prefetching makes use of the `prefetch` instruction, so if we prefetch often while working with sequential memory accesses, it is an extra overhead that we do not need, and might consequentially even hinder performance.
