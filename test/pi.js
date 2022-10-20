/*
 * The Gregory-Leibniz Series
 * https://crypto.stanford.edu/pbc/notes/pi/glseries.html
 */

let pi = 0;
let n = 1;
for (let i = 0; i < 10000; i++) {
    pi += 4 / n;
    n += 2;
    pi -= 4 / n;
    n += 2;
}
pi;
