import pandas
import matplotlib
import matplotlib.pyplot as plt
import numpy as np

df = pandas.read_csv("startup.txt", header=None)
print(df)
df = df[::-1]

fig = plt.figure()
ax1 = fig.add_subplot(1, 1, 1)
ax2 = ax1.twiny()

df.plot.barh(
    x=0,
    y=4,
    label="RSS",
    color="#a98d1980",
    position=1,
    xlabel="RSS (byte)",
    ylabel="runtime",
    ax=ax1,
)
ax1.ticklabel_format(useMathText=True, axis="x")

df.plot.barh(
    x=0,
    y=[1, 2],
    label=["real", "user"],
    xlabel="time (seconds)",
    ylabel="",
    ax=ax2,
)
ax2.set_xlim([0, 1.5])

plt.title("ffmpeg.wasm -version")
plt.tight_layout()
# plt.show()
plt.savefig("startup.png")
