
discord::Core@ core = null;
int lastConnect = 1;

void pre_frame() {
  if (core is null) {
    if (--lastConnect > 0) return;

    // attempt reconnect every 15 seconds:
    lastConnect = 60 * 15;
    @core = discord::Core(725722436351426643);
    if (discord::result != 0) {
      message("discord: connect failed with err=" + fmtInt(discord::result));
      return;
    }

    message("discord: connected!");
  }
  if (core is null) return;

  core.RunCallbacks();
  if (discord::result != 0) {
    message("discord: RunCallbacks() failed with err=" + fmtInt(discord::result));
    return;
  }

}
