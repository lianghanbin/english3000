const { chromium } = require('playwright');
const path = require('path');

(async () => {
  for (const [name, w, h] of [
    ['vertical', 1080, 1920],
    ['horizontal', 1920, 1080],
  ]) {
    const browser = await chromium.launch();
    const ctx = await browser.newContext({
      viewport: { width: w, height: h },
      recordVideo: { dir: 'videos', size: { width: w, height: h } },
    });
    const page = await ctx.newPage();
    await page.goto('file://' + path.join(__dirname, 'scenes.html'));
    await page.waitForTimeout(47000);
    await ctx.close();
    await browser.close();
    console.log('recorded', name);
  }
  console.log('done');
})();
