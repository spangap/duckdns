import { useMenuStore } from 'spangap-browser/stores/menu'
import DuckDnsPanel from '../panels/DuckDnsPanel.vue'

export function registerDuckDns() {
  useMenuStore().register('settings/network/duckdns', 'DuckDNS', { type: 'panel', component: DuckDnsPanel })
}
