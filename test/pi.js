/*
 * The Gregory-Leibniz Series
 * https://crypto.stanford.edu/pbc/notes/pi/glseries.html
 */

function gls(nl) {
    let pi = 0;
    let n = 1;
    for (let i = 0; i < nl; i++) {
        pi += 4 / n;
        n += 2;
        pi -= 4 / n;
        n += 2;
    }
    return pi;
}
pi = gls(10000);
