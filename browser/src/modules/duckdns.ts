import { useMenuStore } from 'spangap-browser/stores/menu'
import DuckDnsPanel from '../panels/DuckDnsPanel.vue'

export function registerDuckDns() {
  useMenuStore().register('settings', 'Settings', [
    { id: 'network', label: 'Network', type: 'submenu',
      children: [
        { id: 'network.duckdns', label: 'DuckDNS', type: 'panel',
          component: DuckDnsPanel },
      ],
    },
  ])
}
