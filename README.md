# WordClock -- a really old project of mine, now on GitHub

This is the repo for my version of the WordClock, which I've build a couple of and gifted to close friends over the years. The [main patent](https://patents.google.com/patent/DE202009017703U1/en) for the wordclock has expired already, so I'm planning to add more and more of my work I did in this field between 2014 and 2020. You can see how old this repo is because the default branch in this repo is still `master` and not `main`.

This is by far my favorite project I've done so far, just because this has been going on for such a long time and started many years before AI was a thing, back when I was ~14 years old.
The code (especially what I've added as the first commit which was the final version I've written completly 100% pre-AI) is thus kinda rough in places. But: It worked for the last 8 years on ~10 clocks I've build.

## Settings in the browser

[`web/`](web/) is a small website to set up a clock over USB, straight from the browser: time, colour, brightness, night window, light sensor, self-test and factory reset. It replaces the old Java tool (now in `archive/`) and needs no installation, just Chrome, Edge or Firefox 151+ on a computer (Web Serial). It works with the original firmware 6.1 and with 6.2.

The preview draws the real front plate (letter outlines extracted from the front plate print file with `tools/extract_frontplate.py`) and lights the same LEDs the firmware would.

It is published to GitHub Pages by `.github/workflows/pages.yml` (repository settings → Pages → Source: "GitHub Actions"). To try it locally, serve `web/` from `localhost`, e.g. `python3 -m http.server -d web`.
