# ToDo

## Usericons

Usericon paths should be resolved by teh uid in the iconset.xml. That doesn't seem to be the case.

## 2525 icons text vertical shift issue

2525 icons are generated with text bein shifted vertically upwards a little. That's an issue of the icon generator, not Louhi. The problem is visible in the SVGs on disk. Example: safp-----------.svg, sffp-----------.svg. The "SOF" text should be vertically centered but appears slightly above center.

## osgEarth

### settings

- Add icon size/scale setting to osgEarth map plugin settings.

### display

Find out why the hell some icons are displayed small and semi transparent. That looks like some deconfliction thing, especially as the icons start to pop to normal size as I zoom in. Find out how to turn it off and turn it off.
