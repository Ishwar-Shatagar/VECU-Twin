/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      colors: {
        // Automotive dark theme palette
        surface: {
          950: '#0a0c10',
          900: '#0f1218',
          800: '#161b24',
          700: '#1e2530',
          600: '#252e3c',
        },
        brand: {
          400: '#f59e0b',
          500: '#d97706',
          600: '#b45309',
        },
        accent: {
          blue:   '#3b82f6',
          green:  '#10b981',
          red:    '#ef4444',
          orange: '#f97316',
          purple: '#8b5cf6',
        },
      },
      fontFamily: {
        sans: ['Inter', 'system-ui', 'sans-serif'],
        mono: ['JetBrains Mono', 'Fira Code', 'monospace'],
      },
      animation: {
        'pulse-slow':  'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'fade-in':     'fadeIn 0.3s ease-in-out',
        'slide-up':    'slideUp 0.3s ease-out',
      },
      keyframes: {
        fadeIn:  { '0%': { opacity: '0' }, '100%': { opacity: '1' } },
        slideUp: { '0%': { transform: 'translateY(8px)', opacity: '0' },
                   '100%': { transform: 'translateY(0)', opacity: '1' } },
      },
    },
  },
  plugins: [],
}
