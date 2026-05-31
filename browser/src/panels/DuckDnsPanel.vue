<template>
  <div class="q-gutter-y-md">
    <div class="q-gutter-y-sm">
      <SettingText label="Subdomain" k="s.duckdns.domain" />
      <SettingText label="Token" k="s.duckdns.token" />
      <div class="text-caption text-grey-5" style="line-height: 1.35">
        Only the subdomain (e.g. <code>myname</code> for <code>myname.duckdns.org</code>).
        Token from the DuckDNS dashboard. Crontab usually runs <code>duckdns update</code> when the network is up.
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
</script>
