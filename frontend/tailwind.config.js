/** @type {import('tailwindcss').Config} */
module.exports = {
  darkMode: 'class',
  content: [
    './app/**/*.{js,jsx}',
    './components/**/*.{js,jsx}',
    './lib/**/*.{js,jsx}',
  ],
  theme: {
    extend: {
      fontFamily: {
        headline: ['"Space Grotesk"', 'system-ui', 'sans-serif'],
        body: ['Inter', 'system-ui', 'sans-serif'],
        sans: ['Inter', 'system-ui', 'sans-serif'],
        label: ['Inter', 'system-ui', 'sans-serif'],
        mono: ['"JetBrains Mono"', '"Fira Code"', 'monospace'],
      },
      colors: {
        // Stitch MD3 dark palette
        'background':               '#131315',
        'surface-dim':              '#131315',
        'surface':                  '#131315',
        'surface-container-lowest': '#0e0e10',
        'surface-container-low':    '#1c1b1d',
        'surface-container':        '#201f22',
        'surface-container-high':   '#2a2a2c',
        'surface-container-highest':'#353437',
        'surface-variant':          '#353437',
        'surface-bright':           '#39393b',
        'inverse-surface':          '#e5e1e4',
        'surface-tint':             '#adc6ff',

        'on-surface':               '#e5e1e4',
        'on-surface-variant':       '#c2c6d6',
        'on-background':            '#e5e1e4',
        'inverse-on-surface':       '#313032',

        'outline':                  '#8c909f',
        'outline-variant':          '#424754',

        'primary':                  '#adc6ff',
        'on-primary':               '#002e6a',
        'primary-container':        '#4d8eff',
        'on-primary-container':     '#00285d',
        'primary-fixed':            '#d8e2ff',
        'primary-fixed-dim':        '#adc6ff',
        'on-primary-fixed':         '#001a42',
        'on-primary-fixed-variant': '#004395',
        'inverse-primary':          '#005ac2',

        'secondary':                '#d0bcff',
        'on-secondary':             '#3c0091',
        'secondary-container':      '#571bc1',
        'on-secondary-container':   '#c4abff',
        'secondary-fixed':          '#e9ddff',
        'secondary-fixed-dim':      '#d0bcff',
        'on-secondary-fixed':       '#23005c',
        'on-secondary-fixed-variant':'#5516be',

        'tertiary':                 '#4cd7f6',
        'on-tertiary':              '#003640',
        'tertiary-container':       '#009eb9',
        'on-tertiary-container':    '#002f38',
        'tertiary-fixed':           '#acedff',
        'tertiary-fixed-dim':       '#4cd7f6',
        'on-tertiary-fixed':        '#001f26',
        'on-tertiary-fixed-variant':'#004e5c',

        'error':                    '#ffb4ab',
        'on-error':                 '#690005',
        'error-container':          '#93000a',
        'on-error-container':       '#ffdad6',

        // Legacy brand aliases (keep for backward compat)
        brand: {
          50:  '#eef4ff',
          100: '#d8e2ff',
          200: '#adc6ff',
          300: '#8db5ff',
          400: '#4d8eff',
          500: '#005ac2',
          600: '#004395',
          700: '#00285d',
          800: '#001a42',
          900: '#001236',
          950: '#000a24',
        },
      },
      borderRadius: {
        DEFAULT: '0.25rem',
        lg: '0.5rem',
        xl: '0.75rem',
        '2xl': '1rem',
        full: '9999px',
      },
      animation: {
        'pulse-slow':  'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'fade-in':     'fadeIn 0.5s ease-out',
        'slide-up':    'slideUp 0.3s ease-out',
        'slide-down':  'slideDown 0.3s ease-out',
        'glow':        'glow 2s ease-in-out infinite alternate',
        'status-pulse':'statusPulse 2s ease-in-out infinite',
      },
      keyframes: {
        fadeIn: {
          '0%':   { opacity: '0', transform: 'translateY(10px)' },
          '100%': { opacity: '1', transform: 'translateY(0)' },
        },
        slideUp: {
          '0%':   { opacity: '0', transform: 'translateY(20px)' },
          '100%': { opacity: '1', transform: 'translateY(0)' },
        },
        slideDown: {
          '0%':   { opacity: '0', transform: 'translateY(-10px)' },
          '100%': { opacity: '1', transform: 'translateY(0)' },
        },
        glow: {
          '0%':   { boxShadow: '0 0 5px rgba(173,198,255,0.2)' },
          '100%': { boxShadow: '0 0 20px rgba(173,198,255,0.4)' },
        },
        statusPulse: {
          '0%, 100%': { opacity: '1', transform: 'scale(1)' },
          '50%':      { opacity: '0.5', transform: 'scale(1.2)' },
        },
      },
    },
  },
  plugins: [],
};
