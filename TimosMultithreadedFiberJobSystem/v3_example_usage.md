# V3 Example Usage

In order to start up something like a game a few major steps are needed:

- Set up windowing
- Set up rendering toolchain
- Set up physics engine
- Set up audio engine
- Load settings
- Load assets
- Load level
- (if in editor mode) Load up editor UI and tools

So I guess just from thinking about it for a bit, I think that there needs to be a way to add and remove job sources to perform setup and teardown kinda stuff. Ideally this would be just atomic too and can just have some kind of usage metric.

## 
